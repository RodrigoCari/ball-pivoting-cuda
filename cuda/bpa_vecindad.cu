// Integrantes: IVAN ALEXANDER LOPEZ ZEGARRA | JESUS BRUNO CHANCAYAURI SONCCO | JOSE RODRIGO CARI ALMIRON
// Curso: Computación Gráfica - Laboratorio Ball Pivoting
#include "bpa_cuda_api.h"
#include <cuda_runtime.h>
#include <chrono>
#include <climits>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

void check(cudaError_t code, const char* where) {
    if (code != cudaSuccess) {
        std::ostringstream out;
        out << where << ": " << cudaGetErrorString(code);
        throw std::runtime_error(out.str());
    }
}

template<class T> class DeviceMemory {
public:
    T* data = nullptr;
    explicit DeviceMemory(size_t n) {
        if (n) check(cudaMalloc(reinterpret_cast<void**>(&data), n * sizeof(T)), "cudaMalloc");
    }
    DeviceMemory(const DeviceMemory&) = delete;
    DeviceMemory& operator=(const DeviceMemory&) = delete;
    ~DeviceMemory() { if (data) cudaFree(data); }
};

class Event {
public:
    cudaEvent_t value = nullptr;
    Event() { check(cudaEventCreate(&value), "cudaEventCreate"); }
    Event(const Event&) = delete;
    ~Event() { if (value) cudaEventDestroy(value); }
};

// Cada hilo calcula el numero de vecinos de un punto i. Se excluye i == j.
__global__ void contar_vecinos(const float* xyz, int n, float limite,
                               int* cuentas) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float x = xyz[3 * i], y = xyz[3 * i + 1], z = xyz[3 * i + 2];
    int cantidad = 0;
    for (int j = 0; j < n; ++j) {
        if (j == i) continue;
        float dx = x - xyz[3 * j];
        float dy = y - xyz[3 * j + 1];
        float dz = z - xyz[3 * j + 2];
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d <= limite) ++cantidad;
    }
    cuentas[i] = cantidad;
}

// Cada hilo llena un rango exclusivo de la tabla CSR. No requiere atomicos.
// El recorrido creciente de j reproduce el orden de vecinos del codigo CPU.
__global__ void escribir_vecinos(const float* xyz, int n, float limite,
                                 const int* offsets, int* indices) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float x = xyz[3 * i], y = xyz[3 * i + 1], z = xyz[3 * i + 2];
    int cursor = offsets[i];
    for (int j = 0; j < n; ++j) {
        if (j == i) continue;
        float dx = x - xyz[3 * j];
        float dy = y - xyz[3 * j + 1];
        float dz = z - xyz[3 * j + 2];
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d <= limite) indices[cursor++] = j;
    }
}

} // namespace

bool bpa_cuda_neighborhood(const std::vector<float>& xyz, float radius,
                           std::vector<std::vector<int>>& vecinos,
                           double& total_ms, double& kernels_ms,
                           std::string& error) {
    total_ms = 0.0;
    kernels_ms = 0.0;
    error.clear();
    try {
        if (xyz.size() % 3 != 0 || xyz.empty() || radius <= 0.0f)
            throw std::runtime_error("XYZ vacio/invalido o radio no positivo");
        size_t size = xyz.size() / 3;
        if (size > static_cast<size_t>(INT_MAX))
            throw std::runtime_error("Demasiados puntos para offsets de 32 bits");
        const int n = static_cast<int>(size);
        int devices = 0;
        check(cudaGetDeviceCount(&devices), "cudaGetDeviceCount");
        if (devices < 1) throw std::runtime_error("CUDA no detecto una GPU utilizable en WSL2");
        check(cudaSetDevice(0), "cudaSetDevice");

        auto t0 = std::chrono::steady_clock::now();
        DeviceMemory<float> d_xyz(xyz.size());
        DeviceMemory<int> d_cuentas(n);
        check(cudaMemcpy(d_xyz.data, xyz.data(), xyz.size() * sizeof(float),
                         cudaMemcpyHostToDevice), "copia XYZ CPU->GPU");
        Event e0, e1, e2, e3;
        const int threads = 128;
        const int blocks = (n + threads - 1) / threads;
        const float limite = 2.0f * radius;
        check(cudaEventRecord(e0.value), "evento inicial");
        contar_vecinos<<<blocks, threads>>>(d_xyz.data, n, limite, d_cuentas.data);
        check(cudaGetLastError(), "lanzar contar_vecinos");
        check(cudaEventRecord(e1.value), "evento despues de contar");
        std::vector<int> counts(n);
        check(cudaMemcpy(counts.data(), d_cuentas.data, n * sizeof(int),
                         cudaMemcpyDeviceToHost), "copia conteos GPU->CPU");

        std::vector<int> offsets(static_cast<size_t>(n) + 1, 0);
        for (int i = 0; i < n; ++i) {
            if (counts[i] < 0 || static_cast<long long>(offsets[i]) + counts[i] > INT_MAX)
                throw std::runtime_error("Desbordamiento de memoria de vecinos (CSR)");
            offsets[i + 1] = offsets[i] + counts[i];
        }
        DeviceMemory<int> d_offsets(offsets.size());
        DeviceMemory<int> d_indices(offsets.back());
        check(cudaMemcpy(d_offsets.data, offsets.data(), offsets.size() * sizeof(int),
                         cudaMemcpyHostToDevice), "copia offsets CPU->GPU");
        check(cudaEventRecord(e2.value), "evento antes de llenar");
        escribir_vecinos<<<blocks, threads>>>(d_xyz.data, n, limite,
                                              d_offsets.data, d_indices.data);
        check(cudaGetLastError(), "lanzar escribir_vecinos");
        check(cudaEventRecord(e3.value), "evento despues de llenar");
        std::vector<int> indices(offsets.back());
        if (!indices.empty()) {
            check(cudaMemcpy(indices.data(), d_indices.data,
                             indices.size() * sizeof(int), cudaMemcpyDeviceToHost),
                  "copia indices GPU->CPU");
        }
        check(cudaEventSynchronize(e3.value), "sincronizacion CUDA");
        float primera = 0.0f, segunda = 0.0f;
        check(cudaEventElapsedTime(&primera, e0.value, e1.value), "tiempo conteo");
        check(cudaEventElapsedTime(&segunda, e2.value, e3.value), "tiempo escritura");
        kernels_ms = static_cast<double>(primera) + static_cast<double>(segunda);

        vecinos.clear();
        vecinos.resize(n);
        for (int i = 0; i < n; ++i) {
            vecinos[i].assign(indices.begin() + offsets[i],
                              indices.begin() + offsets[i + 1]);
        }
        auto t1 = std::chrono::steady_clock::now();
        total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return true;
    } catch (const std::exception& ex) {
        vecinos.clear();
        error = ex.what();
        return false;
    }
}

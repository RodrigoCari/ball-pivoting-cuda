// Integrantes: IVAN ALEXANDER LOPEZ ZEGARRA | JESUS BRUNO CHANCAYAURI SONCCO | JOSE RODRIGO CARI ALMIRON
// Curso: Computación Gráfica - Laboratorio Ball Pivoting
#pragma once
#include <string>
#include <vector>

// Entrada XYZ intercalada [x0,y0,z0, x1,y1,z1, ...].
// La salida vecinos[i] conserva el orden creciente de j, como el codigo CPU.
// total_ms incluye copias CPU/GPU, kernels y reconstruccion de las listas CPU.
// kernels_ms incluye exclusivamente los dos kernels CUDA.
bool bpa_cuda_neighborhood(const std::vector<float>& xyz, float radius,
                           std::vector<std::vector<int>>& vecinos,
                           double& total_ms, double& kernels_ms,
                           std::string& error);

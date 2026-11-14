// Integrantes: IVAN ALEXANDER LOPEZ ZEGARRA | JESUS BRUNO CHANCAYAURI SONCCO | JOSE RODRIGO CARI ALMIRON
// Curso: Computación Gráfica - Laboratorio Ball Pivoting
#include "bpa_cuda_api.h"
#define BPA_USE_CUDA
double cuda_vecindad_ms = 0.0;
double cuda_kernels_ms = 0.0;
#include <chrono>
#include <iomanip>
#include <stdexcept>
inline void glutPostRedisplay() {}
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace std;

const float PI  = 3.14159265358979323846f;
const float EPS = 1e-12f;

struct Punto
{
    float x, y, z;
};

struct Triangulo
{
    int a, b, c;
};

struct AristaActiva
{
    int a, b;
    Punto centro;
};

struct ResultadoPivot
{
    int punto;
    Punto centro;
    float angulo;
    bool encontrado;
};

vector<Punto> puntos;
vector<Triangulo> malla;
vector<vector<int> > vecinosBPA;

Punto centroModelo = {0,0,0};

float radioBPA = 0.007f;

float rotX = 15.0f;
float rotY = -25.0f;
float escala = 12.0f;

bool arrastrando = false;
int mouseX = 0;
int mouseY = 0;

int modo = 1;
// 0 = puntos
// 1 = malla alambrada
// 2 = malla solida


// ============================================================
// Operaciones vectoriales
// ============================================================

Punto sumar(const Punto& A, const Punto& B)
{
    return {A.x+B.x, A.y+B.y, A.z+B.z};
}

Punto restar(const Punto& A, const Punto& B)
{
    return {A.x-B.x, A.y-B.y, A.z-B.z};
}

Punto multiplicar(const Punto& A, float k)
{
    return {A.x*k, A.y*k, A.z*k};
}

float productoPunto(const Punto& A, const Punto& B)
{
    return A.x*B.x + A.y*B.y + A.z*B.z;
}

Punto productoCruz(const Punto& A, const Punto& B)
{
    return {
        A.y*B.z - A.z*B.y,
        A.z*B.x - A.x*B.z,
        A.x*B.y - A.y*B.x
    };
}

float norma(const Punto& A)
{
    return sqrt(productoPunto(A,A));
}

Punto normalizar(const Punto& A)
{
    float n = norma(A);

    if (n < EPS)
        return {0,0,0};

    return {A.x/n, A.y/n, A.z/n};
}

float distancia(const Punto& A, const Punto& B)
{
    return norma(restar(A,B));
}


// ============================================================
// Cargar nube XYZ
// ============================================================

bool cargarXYZ(const string& nombre)
{
    ifstream archivo(nombre.c_str());

    if (!archivo)
        return false;

    puntos.clear();

    string linea;

    while (getline(archivo, linea))
    {
        if (linea.empty() || linea[0] == '#')
            continue;

        stringstream ss(linea);

        Punto p;

        if (ss >> p.x >> p.y >> p.z)
            puntos.push_back(p);
    }

    return !puntos.empty();
}



// ============================================================
// Calcular centro del modelo para visualizarlo centrado
// ============================================================

void calcularCentroModelo()
{
    if (puntos.empty())
        return;

    float minX = puntos[0].x;
    float maxX = puntos[0].x;

    float minY = puntos[0].y;
    float maxY = puntos[0].y;

    float minZ = puntos[0].z;
    float maxZ = puntos[0].z;

    for (const Punto& p : puntos)
    {
        minX = min(minX, p.x);
        maxX = max(maxX, p.x);

        minY = min(minY, p.y);
        maxY = max(maxY, p.y);

        minZ = min(minZ, p.z);
        maxZ = max(maxZ, p.z);
    }

    centroModelo.x = (minX + maxX) / 2.0f;
    centroModelo.y = (minY + maxY) / 2.0f;
    centroModelo.z = (minZ + maxZ) / 2.0f;
}

// ============================================================
// Geometria de esfera por 3 puntos
// ============================================================

bool centrosEsfera(const Punto& A,
                   const Punto& B,
                   const Punto& C,
                   float radio,
                   Punto& S1,
                   Punto& S2)
{
    Punto u = restar(B,A);
    Punto v = restar(C,A);
    Punto w = productoCruz(u,v);

    float ww = productoPunto(w,w);

    if (ww < EPS)
        return false;

    float uu = productoPunto(u,u);
    float vv = productoPunto(v,v);

    Punto vxw = productoCruz(v,w);
    Punto wxu = productoCruz(w,u);

    Punto O;

    O.x = A.x + (uu*vxw.x + vv*wxu.x)/(2.0f*ww);
    O.y = A.y + (uu*vxw.y + vv*wxu.y)/(2.0f*ww);
    O.z = A.z + (uu*vxw.z + vv*wxu.z)/(2.0f*ww);

    float Rc = distancia(O,A);

    if (Rc > radio + EPS)
        return false;

    float h2 = radio*radio - Rc*Rc;

    if (h2 < 0.0f)
        h2 = 0.0f;

    float h = sqrt(h2);

    Punto n = normalizar(w);

    S1 = sumar(O, multiplicar(n, h));
    S2 = sumar(O, multiplicar(n,-h));

    return true;
}



// ============================================================
// Construir vecindad local para acelerar BPA
// Un punto interior a una esfera que pasa por A se encuentra
// necesariamente a distancia <= 2r de A.
// ============================================================


// El kernel CUDA calcula exactamente el mismo criterio d(Pi,Pj) <= 2*radio.
// La frontera activa y la triangulacion siguen en la CPU.
void construirVecindad(float radio)
{
    std::vector<float> xyz;
    xyz.reserve(3 * puntos.size());
    for (const Punto& p : puntos) {
        xyz.push_back(p.x); xyz.push_back(p.y); xyz.push_back(p.z);
    }
    std::string error;
    if (!bpa_cuda_neighborhood(xyz, radio, vecinosBPA,
                               cuda_vecindad_ms, cuda_kernels_ms, error)) {
        throw std::runtime_error("Construccion CUDA fallida: " + error);
    }
    std::cout << "Vecindad construida por CUDA para radio " << radio << std::endl;
}

// ============================================================
// Esfera vacia
// ============================================================

bool esferaVacia(const Punto& centro,
                 float radio,
                 int ia,
                 int ib,
                 int ic)
{
    float limite = radio - 1e-6f;

    const vector<int>& candidatos = vecinosBPA[ia];

    for (int i : candidatos)
    {
        if (i == ia || i == ib || i == ic)
            continue;

        if (distancia(centro, puntos[i]) < limite)
            return false;
    }

    return true;
}


// ============================================================
// Angulo de pivotamiento alrededor de una arista AB
// ============================================================

float anguloPivote(const Punto& A,
                   const Punto& B,
                   const Punto& centroActual,
                   const Punto& centroNuevo)
{
    Punto eje = normalizar(restar(B,A));

    Punto M = multiplicar(sumar(A,B), 0.5f);

    Punto u = restar(centroActual,M);
    Punto v = restar(centroNuevo,M);

    u = restar(u, multiplicar(eje, productoPunto(u,eje)));
    v = restar(v, multiplicar(eje, productoPunto(v,eje)));

    float nu = norma(u);
    float nv = norma(v);

    if (nu < EPS || nv < EPS)
        return 1e30f;

    u = multiplicar(u,1.0f/nu);
    v = multiplicar(v,1.0f/nv);

    float seno = productoPunto(eje, productoCruz(u,v));
    float coseno = productoPunto(u,v);

    float angulo = atan2(seno,coseno);

    if (angulo <= EPS)
        angulo += 2.0f*PI;

    return angulo;
}


// ============================================================
// Claves canonicas para evitar duplicados
// ============================================================

pair<int,int> claveArista(int a, int b)
{
    if (a > b)
        swap(a,b);

    return {a,b};
}

tuple<int,int,int> claveTriangulo(int a, int b, int c)
{
    int v[3] = {a,b,c};

    sort(v,v+3);

    return make_tuple(v[0],v[1],v[2]);
}


// ============================================================
// Buscar triangulo semilla
// ============================================================

bool buscarSemillaNueva(
                   float radio,
                   const set<tuple<int,int,int> >& triangulosUsados,
                   const vector<bool>& puntoCubierto,
                   Triangulo& semilla,
                   Punto& centro)
{
    const int MAX_VECINOS = 40;

    for (int i = 0; i < (int)puntos.size(); i++)
    {
        // Cada nueva region debe comenzar desde un punto
        // que todavia no haya sido cubierto por la malla.
        if (puntoCubierto[i])
            continue;

        vector<pair<float,int> > vecinos;

        for (int j : vecinosBPA[i])
        {
            vecinos.push_back({
                distancia(puntos[i], puntos[j]),
                j
            });
        }

        sort(vecinos.begin(), vecinos.end());

        if ((int)vecinos.size() > MAX_VECINOS)
            vecinos.resize(MAX_VECINOS);

        for (int a = 0; a < (int)vecinos.size(); a++)
        {
            for (int b = a + 1; b < (int)vecinos.size(); b++)
            {
                int j = vecinos[a].second;
                int k = vecinos[b].second;

                tuple<int,int,int> clave =
                    claveTriangulo(i,j,k);

                if (triangulosUsados.count(clave))
                    continue;

                Punto S1, S2;

                if (!centrosEsfera(
                    puntos[i],
                    puntos[j],
                    puntos[k],
                    radio,
                    S1,
                    S2))
                {
                    continue;
                }

                if (esferaVacia(S1, radio, i, j, k))
                {
                    semilla = {i, j, k};
                    centro = S1;
                    return true;
                }

                if (esferaVacia(S2, radio, i, j, k))
                {
                    semilla = {i, j, k};
                    centro = S2;
                    return true;
                }
            }
        }
    }

    return false;
}


// ============================================================
// Buscar siguiente punto al pivotar alrededor de AB
// ============================================================

ResultadoPivot buscarPuntoBPA(int a,
                              int b,
                              const Punto& centroActual,
                              float radio,
                              const set<tuple<int,int,int> >& triangulosUsados)
{
    ResultadoPivot mejor;

    mejor.punto = -1;
    mejor.angulo = 1e30f;
    mejor.encontrado = false;

    const Punto& A = puntos[a];
    const Punto& B = puntos[b];

    for (int i : vecinosBPA[a])
    {
        if (i == a || i == b)
            continue;

        if (distancia(B,puntos[i]) > 2.0f*radio)
            continue;

        if (triangulosUsados.count(claveTriangulo(a,b,i)))
            continue;

        Punto S1,S2;

        if (!centrosEsfera(
            A,
            B,
            puntos[i],
            radio,
            S1,S2))
        {
            continue;
        }

        Punto centros[2] = {S1,S2};

        for (int k = 0; k < 2; k++)
        {
            Punto S = centros[k];

            if (!esferaVacia(S,radio,a,b,i))
                continue;

            float angulo =
                anguloPivote(A,B,centroActual,S);

            if (angulo < mejor.angulo)
            {
                mejor.punto = i;
                mejor.centro = S;
                mejor.angulo = angulo;
                mejor.encontrado = true;
            }
        }
    }

    return mejor;
}


// ============================================================
// Ball Pivoting pedagogico
// ============================================================

void generarMallaBPA(float radio)
{
    malla.clear();

    cout << endl;
    cout << "Generando malla BPA multi-semilla..." << endl;
    cout << "Radio = " << radio << endl;

    construirVecindad(radio);

    set<tuple<int,int,int> > triangulosUsados;
    vector<bool> puntoCubierto(puntos.size(), false);

    int regiones = 0;

    // Una malla triangular cerrada suele tener aproximadamente 2V caras.
    // Usamos 3V como limite de seguridad.
    const int MAX_TRIANGULOS =
        max(15000, (int)puntos.size() * 3);

    while ((int)malla.size() < MAX_TRIANGULOS)
    {
        Triangulo semilla;
        Punto centroSemilla;

        if (!buscarSemillaNueva(
            radio,
            triangulosUsados,
            puntoCubierto,
            semilla,
            centroSemilla))
        {
            break;
        }

        regiones++;

        queue<AristaActiva> frontera;

        // Cada región tiene su propio conjunto de aristas cerradas.
        set<pair<int,int> > aristasCerradas;

        tuple<int,int,int> ks =
            claveTriangulo(
                semilla.a,
                semilla.b,
                semilla.c
            );

        if (triangulosUsados.count(ks))
            continue;

        malla.push_back(semilla);
        triangulosUsados.insert(ks);

        puntoCubierto[semilla.a] = true;
        puntoCubierto[semilla.b] = true;
        puntoCubierto[semilla.c] = true;

        frontera.push({
            semilla.a,
            semilla.b,
            centroSemilla
        });

        frontera.push({
            semilla.b,
            semilla.c,
            centroSemilla
        });

        frontera.push({
            semilla.c,
            semilla.a,
            centroSemilla
        });

        int triangulosRegion = 1;

        while (!frontera.empty() &&
               (int)malla.size() < MAX_TRIANGULOS)
        {
            AristaActiva e = frontera.front();
            frontera.pop();

            pair<int,int> clave =
                claveArista(e.a,e.b);

            if (aristasCerradas.count(clave))
                continue;

            ResultadoPivot r =
                buscarPuntoBPA(
                    e.a,
                    e.b,
                    e.centro,
                    radio,
                    triangulosUsados
                );

            if (!r.encontrado)
            {
                aristasCerradas.insert(clave);
                continue;
            }

            Triangulo t = {
                e.a,
                e.b,
                r.punto
            };

            tuple<int,int,int> kt =
                claveTriangulo(
                    t.a,
                    t.b,
                    t.c
                );

            if (triangulosUsados.count(kt))
            {
                aristasCerradas.insert(clave);
                continue;
            }

            malla.push_back(t);
            triangulosUsados.insert(kt);
            triangulosRegion++;

            puntoCubierto[t.a] = true;
            puntoCubierto[t.b] = true;
            puntoCubierto[t.c] = true;

            // La arista por la cual se pivoto queda cerrada.
            aristasCerradas.insert(clave);

            pair<int,int> e1 =
                claveArista(t.b,t.c);

            pair<int,int> e2 =
                claveArista(t.c,t.a);

            if (!aristasCerradas.count(e1))
            {
                frontera.push({
                    t.b,
                    t.c,
                    r.centro
                });
            }

            if (!aristasCerradas.count(e2))
            {
                frontera.push({
                    t.c,
                    t.a,
                    r.centro
                });
            }
        }

        if (regiones <= 20 || regiones % 50 == 0)
        {
            cout << "Region " << regiones
                 << ": " << triangulosRegion
                 << " triangulos" << endl;
        }
    }

    int cubiertos = 0;

    for (bool usado : puntoCubierto)
    {
        if (usado)
            cubiertos++;
    }

    cout << endl;
    cout << "Reconstruccion terminada." << endl;
    cout << "Regiones encontradas: "
         << regiones << endl;
    cout << "Puntos cubiertos: "
         << cubiertos << " / "
         << puntos.size() << endl;
    cout << "Triangulos generados: "
         << malla.size() << endl;

    if ((int)malla.size() >= MAX_TRIANGULOS)
    {
        cout << "Se alcanzo el limite de seguridad de "
             << MAX_TRIANGULOS
             << " triangulos." << endl;
    }

    modo = 1;

    glutPostRedisplay();
}



// ============================================================
// Guardar malla generada en OBJ
// ============================================================

bool guardarOBJ(const string& nombre)
{
    if (malla.empty())
    {
        cout << "No hay malla para guardar." << endl;
        return false;
    }

    ofstream archivo(nombre.c_str());

    if (!archivo)
    {
        cout << "No se pudo crear: " << nombre << endl;
        return false;
    }

    archivo << "# Stanford Bunny reconstruido con BPA\n";
    archivo << "# Vertices: " << puntos.size() << "\n";
    archivo << "# Triangulos: " << malla.size() << "\n\n";

    for (const Punto& p : puntos)
        archivo << "v " << p.x << " " << p.y << " " << p.z << "\n";

    archivo << "\n";

    for (const Triangulo& t : malla)
        archivo << "f "
                << t.a + 1 << " "
                << t.b + 1 << " "
                << t.c + 1 << "\n";

    archivo.close();

    cout << "Malla guardada en: " << nombre << endl;

    return true;
}

// ============================================================
// Dibujo
// ============================================================


#include <array>
#include <cstdint>

// Huella de las caras sin depender de su orden ni de la orientacion.
// Un hash igual es una verificacion rapida; no constituye prueba matematica.
std::string huellaMalla() {
    std::vector<std::array<int,3>> caras;
    caras.reserve(malla.size());
    for (const Triangulo& t : malla) {
        std::array<int,3> f = {t.a, t.b, t.c};
        std::sort(f.begin(), f.end());
        caras.push_back(f);
    }
    std::sort(caras.begin(), caras.end());
    std::uint64_t h = UINT64_C(14695981039346656037);
    for (const auto& f : caras) {
        for (int v : f) {
            const std::uint32_t num = static_cast<std::uint32_t>(v);
            for (int k = 0; k < 4; ++k) {
                h ^= (num >> (8*k)) & 255U;
                h *= UINT64_C(1099511628211);
            }
        }
    }
    std::ostringstream s;
    s << std::hex << h;
    return s.str();
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Uso: ejecutable ARCHIVO_XYZ RADIO CSV_SALIDA\n";
        return 2;
    }
    float radio = 0;
    try { radio = std::stof(argv[2]); }
    catch (...) { std::cerr << "Radio incorrecto\n"; return 2; }
    if (radio <= 0 || !cargarXYZ(argv[1])) {
        std::cerr << "Error de radio o archivo XYZ: " << argv[1] << "\n";
        return 3;
    }
    std::cout << "Puntos cargados: " << puntos.size() << std::endl;
    auto inicio = std::chrono::steady_clock::now();
    try { generarMallaBPA(radio); }
    catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << "\n"; return 4; }
    auto fin = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(fin-inicio).count();
    if (malla.empty()) { std::cerr << "Malla vacia\n"; return 5; }
    const std::string hash = huellaMalla();
    std::ofstream csv(argv[3], std::ios::app);
    if (!csv) { std::cerr << "Error escribiendo CSV\n"; return 6; }
    csv << puntos.size() << "," << std::fixed << std::setprecision(3) << radio
        << "," << ms << "," << malla.size() << "," << hash;
#ifdef BPA_USE_CUDA
    csv << "," << std::setprecision(4) << cuda_vecindad_ms << "," << cuda_kernels_ms;
    std::cout << "TIEMPO_HIBRIDO_MS: " << ms << "\n"
              << "GPU_VECINDAD_MS: " << std::fixed << std::setprecision(4) << cuda_vecindad_ms << "\n"
              << "GPU_KERNELS_MS: " << cuda_kernels_ms << "\n";
#else
    std::cout << "TIEMPO_CPU_MS: " << ms << "\n";
#endif
    csv << "\n";
    std::cout << "TRIANGULOS: " << malla.size() << "\n"
              << "MALLA_HASH: " << hash << "\n";
    return 0;
}

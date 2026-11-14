// Integrantes: IVAN ALEXANDER LOPEZ ZEGARRA | JESUS BRUNO CHANCAYAURI SONCCO | JOSE RODRIGO CARI ALMIRON
// Curso: Computación Gráfica - Laboratorio Ball Pivoting
#include <GL/glut.h>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

const float PI = 3.14159265358979323846f;
const float RADIO = 80.0f;
const float EPS = 0.001f;

struct Punto
{
    float x, y;
};

vector<Punto> puntos = {
    {100,100},
    {200, 80},
    {300,120},
    {340,220},
    {260,300},
    {150,280},
    { 80,200}
};

vector<int> contorno;

int aActual = 0;
int bActual = 1;
Punto centroActual;
bool terminado = false;


// ------------------------------------------------------------
// Distancia entre dos puntos
// ------------------------------------------------------------
float distancia(const Punto& A, const Punto& B)
{
    float dx = A.x - B.x;
    float dy = A.y - B.y;
    return sqrt(dx*dx + dy*dy);
}


// ------------------------------------------------------------
// Dos centros posibles de un circulo de radio r que pasa
// por los puntos A y B.
// ------------------------------------------------------------
bool centrosCirculo(const Punto& A, const Punto& B, float r,
                    Punto& C1, Punto& C2)
{
    float dx = B.x - A.x;
    float dy = B.y - A.y;
    float d = sqrt(dx*dx + dy*dy);

    if (d > 2.0f*r || d < EPS)
        return false;

    Punto M = {(A.x + B.x)/2.0f, (A.y + B.y)/2.0f};

    float h = sqrt(r*r - (d*d)/4.0f);

    float ux = -dy / d;
    float uy =  dx / d;

    C1 = {M.x + h*ux, M.y + h*uy};
    C2 = {M.x - h*ux, M.y - h*uy};

    return true;
}


// ------------------------------------------------------------
// Comprueba que no exista otro punto dentro del circulo.
// Los puntos i1 e i2 estan sobre el circulo y se ignoran.
// ------------------------------------------------------------
bool circuloVacio(const Punto& centro, float r, int i1, int i2)
{
    for (int i = 0; i < (int)puntos.size(); i++)
    {
        if (i == i1 || i == i2)
            continue;

        if (distancia(centro, puntos[i]) < r - EPS)
            return false;
    }

    return true;
}


// ------------------------------------------------------------
// Angulo positivo antihorario desde el vector u hasta v.
// ------------------------------------------------------------
float anguloCCW(const Punto& u, const Punto& v)
{
    float a1 = atan2(u.y, u.x);
    float a2 = atan2(v.y, v.x);

    float angulo = a2 - a1;

    while (angulo <= EPS)
        angulo += 2.0f*PI;

    return angulo;
}


// ------------------------------------------------------------
// Indica si un punto ya fue usado en el contorno.
// ------------------------------------------------------------
bool puntoUsado(int indice)
{
    for (int i : contorno)
        if (i == indice)
            return true;

    return false;
}


// ------------------------------------------------------------
// Busca el siguiente punto que el circulo encuentra al pivotar.
// En esta version pedagogica evitamos repetir puntos y cerramos
// el contorno cuando todos los puntos ya fueron visitados.
// ------------------------------------------------------------
int buscarSiguiente(Punto& nuevoCentro)
{
    const Punto& B = puntos[bActual];

    Punto vectorActual = {
        centroActual.x - B.x,
        centroActual.y - B.y
    };

    float mejorAngulo = 1e30f;
    int mejorPunto = -1;

    for (int i = 0; i < (int)puntos.size(); i++)
    {
        if (i == aActual || i == bActual)
            continue;

        // No repetir puntos ya recorridos.
        // El punto 0 solo se permite para cerrar al final.
        if (puntoUsado(i))
        {
            if (i != contorno.front())
                continue;

            if ((int)contorno.size() < (int)puntos.size())
                continue;
        }

        Punto C1, C2;

        if (!centrosCirculo(B, puntos[i], RADIO, C1, C2))
            continue;

        Punto candidatos[2] = {C1, C2};

        for (int k = 0; k < 2; k++)
        {
            Punto centro = candidatos[k];

            if (!circuloVacio(centro, RADIO, bActual, i))
                continue;

            Punto vectorNuevo = {
                centro.x - B.x,
                centro.y - B.y
            };

            float angulo = anguloCCW(vectorActual, vectorNuevo);

            if (angulo < mejorAngulo)
            {
                mejorAngulo = angulo;
                mejorPunto = i;
                nuevoCentro = centro;
            }
        }
    }

    return mejorPunto;
}


// ------------------------------------------------------------
// Ejecuta un solo paso del algoritmo.
// ------------------------------------------------------------
void avanzarPaso()
{
    if (terminado)
        return;

    Punto nuevoCentro;
    int siguiente = buscarSiguiente(nuevoCentro);

    if (siguiente == -1)
    {
        cout << "No se encontro otro punto." << endl;
        terminado = true;
        return;
    }

    cout << "Arista (" << aActual << "," << bActual << ")"
         << " -> siguiente P" << siguiente << endl;

    aActual = bActual;
    bActual = siguiente;
    centroActual = nuevoCentro;

    if (siguiente == contorno.front())
    {
        terminado = true;
        cout << "Contorno cerrado." << endl;
    }
    else
    {
        contorno.push_back(siguiente);
    }

    glutPostRedisplay();
}


// ------------------------------------------------------------
// Inicializa el ejemplo.
// ------------------------------------------------------------
void reiniciar()
{
    contorno.clear();
    contorno.push_back(0);
    contorno.push_back(1);

    aActual = 0;
    bActual = 1;
    terminado = false;

    Punto C1, C2;
    centrosCirculo(puntos[0], puntos[1], RADIO, C1, C2);

    // Elegimos el centro inferior para comenzar desde el exterior.
    centroActual = (C1.y < C2.y) ? C1 : C2;

    cout << endl;
    cout << "Reinicio" << endl;
    cout << "Arista inicial: (0,1)" << endl;

    glutPostRedisplay();
}


// ------------------------------------------------------------
// Dibuja un circulo.
// ------------------------------------------------------------
void dibujarCirculo(const Punto& C, float r)
{
    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < 100; i++)
    {
        float angulo = 2.0f*PI*i/100.0f;
        glVertex2f(C.x + r*cos(angulo),
                   C.y + r*sin(angulo));
    }

    glEnd();
}


// ------------------------------------------------------------
// Dibuja texto.
// ------------------------------------------------------------
void dibujarTexto(float x, float y, const string& texto)
{
    glRasterPos2f(x, y);

    for (char c : texto)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
}


// ------------------------------------------------------------
// Dibuja la escena.
// ------------------------------------------------------------
void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Contorno construido
    glColor3f(0.1f, 0.3f, 0.9f);
    glLineWidth(3.0f);

    glBegin(GL_LINE_STRIP);
    for (int indice : contorno)
        glVertex2f(puntos[indice].x, puntos[indice].y);

    if (terminado && bActual == contorno.front())
        glVertex2f(puntos[contorno.front()].x,
                   puntos[contorno.front()].y);
    glEnd();

    // Arista actual
    if (!terminado)
    {
        glColor3f(0.9f, 0.2f, 0.2f);
        glLineWidth(4.0f);

        glBegin(GL_LINES);
        glVertex2f(puntos[aActual].x, puntos[aActual].y);
        glVertex2f(puntos[bActual].x, puntos[bActual].y);
        glEnd();
    }

    // Circulo actual
    glColor3f(0.2f, 0.65f, 0.2f);
    glLineWidth(2.0f);
    dibujarCirculo(centroActual, RADIO);

    // Centro del circulo
    glPointSize(6.0f);
    glBegin(GL_POINTS);
    glVertex2f(centroActual.x, centroActual.y);
    glEnd();

    // Nube de puntos
    glColor3f(0.0f, 0.0f, 0.0f);
    glPointSize(10.0f);

    glBegin(GL_POINTS);
    for (const Punto& p : puntos)
        glVertex2f(p.x, p.y);
    glEnd();

    // Etiquetas P0, P1, ...
    for (int i = 0; i < (int)puntos.size(); i++)
    {
        stringstream ss;
        ss << "P" << i;
        dibujarTexto(puntos[i].x + 7.0f,
                     puntos[i].y + 7.0f,
                     ss.str());
    }

    // Instrucciones
    glColor3f(0.0f, 0.0f, 0.0f);
    dibujarTexto(10, 365, "ESPACIO: avanzar un pivote   R: reiniciar   ESC: salir");

    stringstream estado;

    if (terminado)
        estado << "Contorno terminado";
    else
        estado << "Arista actual: P" << aActual << " - P" << bActual;

    dibujarTexto(10, 345, estado.str());

    glutSwapBuffers();
}


// ------------------------------------------------------------
// Teclado
// ------------------------------------------------------------
void teclado(unsigned char tecla, int, int)
{
    if (tecla == ' ')
        avanzarPaso();

    if (tecla == 'r' || tecla == 'R')
        reiniciar();

    if (tecla == 27)
        exit(0);
}


// ------------------------------------------------------------
// Configuracion de OpenGL
// ------------------------------------------------------------
void reshape(int w, int h)
{
    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, 420, 0, 390);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}


// ------------------------------------------------------------
// Programa principal
// ------------------------------------------------------------
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 700);
    glutCreateWindow("Ball Pivoting 2D - Ejemplo pedagogico");

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    glutDisplayFunc(display);
    glutKeyboardFunc(teclado);
    glutReshapeFunc(reshape);

    reiniciar();

    glutMainLoop();

    return 0;
}

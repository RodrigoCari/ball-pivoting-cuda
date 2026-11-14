// Integrantes: IVAN ALEXANDER LOPEZ ZEGARRA | JESUS BRUNO CHANCAYAURI SONCCO | JOSE RODRIGO CARI ALMIRON
// Curso: Computación Gráfica - Laboratorio Ball Pivoting
#include <GL/glut.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

const float PI = 3.14159265358979323846f;
const float EPS = 0.000001f;
const int TOTAL_PUNTOS = 20;

struct Punto
{
    float x, y, z;
};

struct Triangulo
{
    int a, b, c;
};

vector<Punto> puntos;
vector<int> orden;
vector<Triangulo> malla;

float rotX = 25.0f;
float rotY = -30.0f;

bool arrastrando = false;
int mouseX = 0;
int mouseY = 0;

Punto centroEsferaActual = {0,0,0};
float radioEsferaActual = 1.0f;
bool mostrarEsfera = false;


// ------------------------------------------------------------
// Utilidades
// ------------------------------------------------------------

float aleatorio(float minimo, float maximo)
{
    return minimo + (maximo - minimo) *
           ((float)rand() / (float)RAND_MAX);
}

Punto restar(const Punto& A, const Punto& B)
{
    return {A.x - B.x, A.y - B.y, A.z - B.z};
}

Punto sumar(const Punto& A, const Punto& B)
{
    return {A.x + B.x, A.y + B.y, A.z + B.z};
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
    return sqrt(productoPunto(A, A));
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
    return norma(restar(A, B));
}


// ------------------------------------------------------------
// Generar nube de 20 puntos
//
// P0 es un punto central elevado.
// Los otros 19 puntos se distribuyen aleatoriamente alrededor.
// Se ordenan por angulo para construir una superficie simple.
// ------------------------------------------------------------

void generarNube()
{
    puntos.clear();
    orden.clear();
    malla.clear();

    // Punto central
    puntos.push_back({0.0f, 0.0f, 1.2f});

    struct PuntoAngular
    {
        Punto p;
        float angulo;
    };

    vector<PuntoAngular> temporales;

    for (int i = 1; i < TOTAL_PUNTOS; i++)
    {
        float angulo = aleatorio(0.0f, 2.0f*PI);
        float radio = aleatorio(1.4f, 2.3f);

        float x = radio * cos(angulo);
        float y = radio * sin(angulo);

        // Pequeño relieve aleatorio
        float z = aleatorio(-0.15f, 0.45f);

        temporales.push_back({{x,y,z}, angulo});
    }

    sort(
        temporales.begin(),
        temporales.end(),
        [](const PuntoAngular& A, const PuntoAngular& B)
        {
            return A.angulo < B.angulo;
        }
    );

    for (const PuntoAngular& item : temporales)
    {
        puntos.push_back(item.p);
        orden.push_back((int)puntos.size() - 1);
    }

    mostrarEsfera = false;

    cout << endl;
    cout << "Nueva nube generada con "
         << puntos.size()
         << " puntos." << endl;

    glutPostRedisplay();
}


// ------------------------------------------------------------
// Centros de esfera de radio dado que pasa por A,B,C
// ------------------------------------------------------------

bool centrosEsfera(const Punto& A, const Punto& B, const Punto& C,
                   float radio, Punto& S1, Punto& S2)
{
    Punto u = restar(B, A);
    Punto v = restar(C, A);
    Punto w = productoCruz(u, v);

    float ww = productoPunto(w, w);

    if (ww < EPS)
        return false;

    float uu = productoPunto(u, u);
    float vv = productoPunto(v, v);

    Punto vxw = productoCruz(v, w);
    Punto wxu = productoCruz(w, u);

    Punto O;

    O.x = A.x + (uu*vxw.x + vv*wxu.x)/(2.0f*ww);
    O.y = A.y + (uu*vxw.y + vv*wxu.y)/(2.0f*ww);
    O.z = A.z + (uu*vxw.z + vv*wxu.z)/(2.0f*ww);

    float Rc = distancia(O, A);

    if (Rc > radio)
        return false;

    float h = sqrt(radio*radio - Rc*Rc);
    Punto n = normalizar(w);

    S1 = sumar(O, multiplicar(n, h));
    S2 = sumar(O, multiplicar(n, -h));

    return true;
}


// ------------------------------------------------------------
// Radio circunscrito del triangulo
// ------------------------------------------------------------

float radioCircunscrito(const Punto& A, const Punto& B, const Punto& C)
{
    float a = distancia(B, C);
    float b = distancia(A, C);
    float c = distancia(A, B);

    Punto u = restar(B, A);
    Punto v = restar(C, A);

    float area = 0.5f * norma(productoCruz(u, v));

    if (area < EPS)
        return 0.0f;

    return (a*b*c) / (4.0f*area);
}


// ------------------------------------------------------------
// Esfera alambrada del ultimo triangulo
// ------------------------------------------------------------

void actualizarEsfera(const Triangulo& t)
{
    const Punto& A = puntos[t.a];
    const Punto& B = puntos[t.b];
    const Punto& C = puntos[t.c];

    float Rc = radioCircunscrito(A, B, C);

    if (Rc < EPS)
    {
        mostrarEsfera = false;
        return;
    }

    // Elegimos un radio un poco mayor al circunradio
    radioEsferaActual = Rc + 0.35f;

    Punto S1, S2;

    if (!centrosEsfera(
        A, B, C,
        radioEsferaActual,
        S1, S2))
    {
        mostrarEsfera = false;
        return;
    }

    // Para visualizarla mejor escogemos el centro con mayor z
    centroEsferaActual = (S1.z > S2.z) ? S1 : S2;
    mostrarEsfera = true;
}


// ------------------------------------------------------------
// Agrega un triangulo por cada pulsacion de ESPACIO
// ------------------------------------------------------------

void avanzarPaso()
{
    if (orden.empty())
        return;

    if ((int)malla.size() >= (int)orden.size())
    {
        cout << "Malla completa." << endl;
        return;
    }

    int i = (int)malla.size();

    int a = 0;
    int b = orden[i];
    int c = orden[(i + 1) % orden.size()];

    Triangulo t = {a, b, c};

    malla.push_back(t);
    actualizarEsfera(t);

    cout << "Triangulo T" << i
         << " = (P" << a
         << ", P" << b
         << ", P" << c << ")" << endl;

    glutPostRedisplay();
}


// ------------------------------------------------------------
// Reiniciar solo la malla, manteniendo los mismos puntos
// ------------------------------------------------------------

void reiniciarMalla()
{
    malla.clear();
    mostrarEsfera = false;

    cout << endl;
    cout << "Malla reiniciada." << endl;

    glutPostRedisplay();
}


// ------------------------------------------------------------
// OpenGL
// ------------------------------------------------------------

void dibujarTexto(float x, float y, const string& texto)
{
    glRasterPos2f(x, y);

    for (char c : texto)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
}

void dibujarEjes()
{
    glLineWidth(2.0f);

    glBegin(GL_LINES);

    glColor3f(0.8f,0.1f,0.1f);
    glVertex3f(0,0,0);
    glVertex3f(3,0,0);

    glColor3f(0.1f,0.6f,0.1f);
    glVertex3f(0,0,0);
    glVertex3f(0,3,0);

    glColor3f(0.1f,0.2f,0.8f);
    glVertex3f(0,0,0);
    glVertex3f(0,0,3);

    glEnd();
}

void dibujarPuntos()
{
    glPointSize(8.0f);
    glColor3f(0.05f,0.05f,0.05f);

    glBegin(GL_POINTS);

    for (const Punto& p : puntos)
        glVertex3f(p.x, p.y, p.z);

    glEnd();
}

void dibujarMalla()
{
    glLineWidth(2.0f);

    for (int i = 0; i < (int)malla.size(); i++)
    {
        const Triangulo& t = malla[i];

        if (i == (int)malla.size()-1)
            glColor3f(0.9f,0.15f,0.15f);
        else
            glColor3f(0.1f,0.3f,0.9f);

        glBegin(GL_LINE_LOOP);

        glVertex3f(puntos[t.a].x, puntos[t.a].y, puntos[t.a].z);
        glVertex3f(puntos[t.b].x, puntos[t.b].y, puntos[t.b].z);
        glVertex3f(puntos[t.c].x, puntos[t.c].y, puntos[t.c].z);

        glEnd();
    }
}

void dibujarEsfera()
{
    if (!mostrarEsfera)
        return;

    glPushMatrix();

    glTranslatef(
        centroEsferaActual.x,
        centroEsferaActual.y,
        centroEsferaActual.z
    );

    glColor3f(0.1f,0.65f,0.2f);

    glutWireSphere(
        radioEsferaActual,
        18,
        14
    );

    glPopMatrix();
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(0.0f,0.0f,-8.0f);

    glRotatef(rotX,1,0,0);
    glRotatef(rotY,0,1,0);

    dibujarEjes();
    dibujarMalla();
    dibujarEsfera();
    dibujarPuntos();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    gluOrtho2D(0,800,0,700);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);

    glColor3f(0,0,0);

    dibujarTexto(
        15,
        670,
        "ESPACIO: triangulo   R: reiniciar malla   N: nueva nube   Flechas/Mouse: rotar"
    );

    stringstream ss;

    ss << "Puntos: "
       << puntos.size()
       << "   Triangulos: "
       << malla.size()
       << " / "
       << orden.size();

    dibujarTexto(15,645,ss.str());

    glEnable(GL_DEPTH_TEST);

    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}


// ------------------------------------------------------------
// Controles
// ------------------------------------------------------------

void teclado(unsigned char tecla, int, int)
{
    if (tecla == ' ')
        avanzarPaso();

    if (tecla == 'r' || tecla == 'R')
        reiniciarMalla();

    if (tecla == 'n' || tecla == 'N')
        generarNube();

    if (tecla == 27)
        exit(0);
}

void teclasEspeciales(int tecla, int, int)
{
    if (tecla == GLUT_KEY_LEFT)
        rotY -= 5.0f;

    if (tecla == GLUT_KEY_RIGHT)
        rotY += 5.0f;

    if (tecla == GLUT_KEY_UP)
        rotX -= 5.0f;

    if (tecla == GLUT_KEY_DOWN)
        rotX += 5.0f;

    glutPostRedisplay();
}

void mouse(int boton, int estado, int x, int y)
{
    if (boton == GLUT_LEFT_BUTTON)
    {
        if (estado == GLUT_DOWN)
        {
            arrastrando = true;
            mouseX = x;
            mouseY = y;
        }
        else
        {
            arrastrando = false;
        }
    }
}

void movimientoMouse(int x, int y)
{
    if (!arrastrando)
        return;

    rotY += (x - mouseX)*0.5f;
    rotX += (y - mouseY)*0.5f;

    mouseX = x;
    mouseY = y;

    glutPostRedisplay();
}

void reshape(int w, int h)
{
    if (h == 0)
        h = 1;

    glViewport(0,0,w,h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(
        45.0,
        (double)w/(double)h,
        0.1,
        100.0
    );

    glMatrixMode(GL_MODELVIEW);
}


// ------------------------------------------------------------
// Programa principal
// ------------------------------------------------------------

int main(int argc, char** argv)
{
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);

    glutInitDisplayMode(
        GLUT_DOUBLE |
        GLUT_RGB |
        GLUT_DEPTH
    );

    glutInitWindowSize(800,700);

    glutCreateWindow(
        "Nube aleatoria 3D - 20 puntos"
    );

    glClearColor(1,1,1,1);
    glEnable(GL_DEPTH_TEST);

    glutDisplayFunc(display);
    glutKeyboardFunc(teclado);
    glutSpecialFunc(teclasEspeciales);
    glutMouseFunc(mouse);
    glutMotionFunc(movimientoMouse);
    glutReshapeFunc(reshape);

    generarNube();

    glutMainLoop();

    return 0;
}

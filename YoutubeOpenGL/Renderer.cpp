
#include "Renderer.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <iostream>
#include <string>
#include <memory>
#include <algorithm>
#include <cstring>
#include <cstdint>

// TINYGLTF 2.x
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "legacy_gltf/tiny_gltf.h"

// =====================================
// VARIABLES GENERALES
// =====================================

static double lastMouseX = 400;
static double lastMouseY = 300;
static bool firstMouse = true;

static float bezierT = 0.0f;
static float lastTime = 0.0f;

static const float PI = 3.14159265359f;

static std::unique_ptr<tinygltf::Model> modeloTaza;
static bool tazaCargada = false;

// =====================================
// CARGAR MODELO GLTF
// =====================================

static bool cargarTaza(const std::string& ruta)
{
    tinygltf::TinyGLTF loader;

    std::string error;
    std::string warning;

    std::unique_ptr<tinygltf::Model> modelo(
        new tinygltf::Model()
    );

    bool resultado = false;

    if (ruta.size() >= 4 &&
        ruta.substr(ruta.size() - 4) == ".glb")
    {
        resultado = loader.LoadBinaryFromFile(
            modelo.get(),
            &error,
            &warning,
            ruta
        );
    }
    else
    {
        resultado = loader.LoadASCIIFromFile(
            modelo.get(),
            &error,
            &warning,
            ruta
        );
    }

    if (!warning.empty())
        std::cout << warning << std::endl;

    if (!error.empty())
        std::cerr << error << std::endl;

    if (!resultado)
    {
        tazaCargada = false;
        modeloTaza.reset();

        std::cerr
            << "No se pudo cargar el modelo"
            << std::endl;

        return false;
    }

    modeloTaza = std::move(modelo);
    tazaCargada = true;

    std::cout
        << "Modelo cargado correctamente"
        << std::endl;

    std::cout
        << "Cantidad de mallas: "
        << modeloTaza->meshes.size()
        << std::endl;

    return true;
}

// =====================================
// LEER DATOS GLTF
// =====================================

static size_t tamanoComponente(int tipo)
{
    switch (tipo)
    {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return 1;

    case TINYGLTF_COMPONENT_TYPE_SHORT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return 2;

    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return 4;

    default:
        return 0;
    }
}

static bool obtenerDatos(
    const tinygltf::Model& modelo,
    const tinygltf::Accessor& accessor,
    const unsigned char*& datos,
    size_t& stride)
{
    if (accessor.bufferView < 0 ||
        accessor.sparse.isSparse)
        return false;

    if ((size_t)accessor.bufferView >=
        modelo.bufferViews.size())
        return false;

    const tinygltf::BufferView& vista =
        modelo.bufferViews[accessor.bufferView];

    if (vista.buffer < 0 ||
        (size_t)vista.buffer >= modelo.buffers.size())
        return false;

    const tinygltf::Buffer& buffer =
        modelo.buffers[vista.buffer];

    size_t componente =
        tamanoComponente(accessor.componentType);

    if (componente == 0)
        return false;

    int cantidadComponentes = 1;

    if (accessor.type == TINYGLTF_TYPE_VEC2)
        cantidadComponentes = 2;
    else if (accessor.type == TINYGLTF_TYPE_VEC3)
        cantidadComponentes = 3;
    else if (accessor.type == TINYGLTF_TYPE_VEC4)
        cantidadComponentes = 4;
    else if (accessor.type != TINYGLTF_TYPE_SCALAR)
        return false;

    size_t elemento =
        componente * cantidadComponentes;

    stride = vista.byteStride
        ? vista.byteStride
        : elemento;

    if (stride < elemento)
        return false;

    size_t inicio =
        vista.byteOffset + accessor.byteOffset;

    if (inicio > buffer.data.size())
        return false;

    if (accessor.count > 0)
    {
        size_t disponible =
            buffer.data.size() - inicio;

        if (disponible < elemento)
            return false;

        if (accessor.count - 1 >
            (disponible - elemento) / stride)
            return false;
    }

    datos = buffer.data.data() + inicio;

    return true;
}

static float leerFloat(const unsigned char* p)
{
    float valor;
    std::memcpy(&valor, p, sizeof(float));
    return valor;
}

static unsigned int leerIndice(
    const unsigned char* p,
    int tipo)
{
    if (tipo == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
        return *p;

    if (tipo == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        uint16_t valor;
        std::memcpy(&valor, p, sizeof(valor));
        return valor;
    }

    if (tipo == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
    {
        uint32_t valor;
        std::memcpy(&valor, p, sizeof(valor));
        return valor;
    }

    return 0;
}

// =====================================
// DIBUJAR MALLA
// =====================================

static void dibujarMalla(
    const tinygltf::Model& modelo,
    const tinygltf::Mesh& mesh)
{
    for (const auto& primitive : mesh.primitives)
    {
        auto it =
            primitive.attributes.find("POSITION");

        if (it == primitive.attributes.end())
            continue;

        if (it->second < 0 ||
            (size_t)it->second >= modelo.accessors.size())
            continue;

        const tinygltf::Accessor& posiciones =
            modelo.accessors[it->second];

        if (posiciones.type != TINYGLTF_TYPE_VEC3 ||
            posiciones.componentType !=
            TINYGLTF_COMPONENT_TYPE_FLOAT)
            continue;

        const unsigned char* vertices = nullptr;
        size_t strideVertices = 0;

        if (!obtenerDatos(
            modelo,
            posiciones,
            vertices,
            strideVertices))
            continue;

        const tinygltf::Accessor* accIndices = nullptr;
        const unsigned char* indices = nullptr;
        size_t strideIndices = 0;

        if (primitive.indices >= 0)
        {
            if ((size_t)primitive.indices >=
                modelo.accessors.size())
                continue;

            accIndices =
                &modelo.accessors[primitive.indices];

            if (accIndices->type != TINYGLTF_TYPE_SCALAR)
                continue;

            if (accIndices->componentType !=
                TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE &&
                accIndices->componentType !=
                TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT &&
                accIndices->componentType !=
                TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                continue;

            if (!obtenerDatos(
                modelo,
                *accIndices,
                indices,
                strideIndices))
                continue;
        }

        // Determinar el modo de dibujo
        GLenum modo = GL_TRIANGLES;

        switch (primitive.mode)
        {
        case TINYGLTF_MODE_TRIANGLES:
            modo = GL_TRIANGLES;
            break;

        case TINYGLTF_MODE_TRIANGLE_STRIP:
            modo = GL_TRIANGLE_STRIP;
            break;

        case TINYGLTF_MODE_TRIANGLE_FAN:
            modo = GL_TRIANGLE_FAN;
            break;

        case 1:
            modo = GL_LINES;
            break;

        case TINYGLTF_MODE_LINE_STRIP:
            modo = GL_LINE_STRIP;
            break;

        case TINYGLTF_MODE_POINTS:
            modo = GL_POINTS;
            break;

        default:
            continue;
        }

        glColor3f(0.75f, 0.45f, 0.25f);

        if (primitive.material >= 0 &&
            (size_t)primitive.material <
            modelo.materials.size())
        {
            const auto& color =
                modelo.materials[primitive.material]
                .pbrMetallicRoughness.baseColorFactor;

            if (color.size() >= 3)
            {
                glColor3f(
                    (float)color[0],
                    (float)color[1],
                    (float)color[2]
                );
            }
        }

        size_t cantidad = accIndices
            ? accIndices->count
            : posiciones.count;

        glBegin(modo);

        for (size_t i = 0; i < cantidad; i++)
        {
            size_t indice = i;

            if (accIndices)
            {
                indice = leerIndice(
                    indices + i * strideIndices,
                    accIndices->componentType
                );
            }

            if (indice >= posiciones.count)
                continue;

            const unsigned char* p =
                vertices + indice * strideVertices;

            glVertex3f(
                leerFloat(p),
                leerFloat(p + 4),
                leerFloat(p + 8)
            );
        }

        glEnd();
    }
}

// =====================================
// DIBUJAR NODOS
// =====================================

static void dibujarNodo(
    const tinygltf::Model& modelo,
    int indiceNodo,
    int profundidad = 0)
{
    if (profundidad > 128)
        return;

    if (indiceNodo < 0 ||
        (size_t)indiceNodo >= modelo.nodes.size())
        return;

    const tinygltf::Node& nodo =
        modelo.nodes[indiceNodo];

    glPushMatrix();

    if (nodo.matrix.size() == 16)
    {
        glMultMatrixd(nodo.matrix.data());
    }
    else
    {
        if (nodo.translation.size() == 3)
        {
            glTranslated(
                nodo.translation[0],
                nodo.translation[1],
                nodo.translation[2]
            );
        }

        if (nodo.rotation.size() == 4)
        {
            double qx = nodo.rotation[0];
            double qy = nodo.rotation[1];
            double qz = nodo.rotation[2];
            double qw = nodo.rotation[3];

            double longitud = std::sqrt(
                qx * qx + qy * qy + qz * qz + qw * qw
            );

            if (longitud > 0.000001)
            {
                qx /= longitud;
                qy /= longitud;
                qz /= longitud;
                qw /= longitud;

                double angulo =
                    2.0 * std::acos(
                        (std::max)(
                            -1.0,
                            (std::min)(1.0, qw)
                            )
                    );

                double seno =
                    std::sqrt(
                        (std::max)(0.0, 1.0 - qw * qw)
                    );

                if (seno > 0.000001)
                {
                    glRotated(
                        angulo * 180.0 / PI,
                        qx / seno,
                        qy / seno,
                        qz / seno
                    );
                }
            }
        }

        if (nodo.scale.size() == 3)
        {
            glScaled(
                nodo.scale[0],
                nodo.scale[1],
                nodo.scale[2]
            );
        }
    }

    if (nodo.mesh >= 0 &&
        (size_t)nodo.mesh < modelo.meshes.size())
    {
        dibujarMalla(
            modelo,
            modelo.meshes[nodo.mesh]
        );
    }

    for (int hijo : nodo.children)
    {
        dibujarNodo(
            modelo,
            hijo,
            profundidad + 1
        );
    }

    glPopMatrix();
}

// =====================================
// DIBUJAR MODELO COMPLETO
// =====================================

static void dibujarModeloGLTF()
{
    if (!tazaCargada || !modeloTaza)
        return;

    const tinygltf::Model& modelo = *modeloTaza;

    int escena = modelo.defaultScene;

    if (escena < 0 && !modelo.scenes.empty())
        escena = 0;

    if (escena < 0 ||
        (size_t)escena >= modelo.scenes.size())
        return;

    for (int nodo : modelo.scenes[escena].nodes)
    {
        dibujarNodo(modelo, nodo);
    }
}


// =====================================
// CURVA BEZIER
// =====================================

static void calcularBezier(
    float t,
    float& x,
    float& y,
    float& z)
{
    float u = 1.0f - t;

    x =
        u * u * u * (-2.0f) +
        3.0f * u * u * t * (-1.0f) +
        3.0f * u * t * t * (1.0f) +
        t * t * t * (2.0f);

    y = 0.0f;

    z =
        u * u * u * (-1.25f) +
        3.0f * u * u * t * (2.0f) +
        3.0f * u * t * t * (-2.0f) +
        t * t * t * (1.25f);
}

static void drawBezierCurve()
{
    glColor3f(1.0f, 1.0f, 0.0f);

    glBegin(GL_LINE_STRIP);

    for (int i = 0; i <= 100; i++)
    {
        float t = i / 100.0f;

        float x, y, z;

        calcularBezier(t, x, y, z);

        glVertex3f(x, y, z);
    }

    glEnd();
}

// =====================================
// CONSTRUCTOR
// =====================================

Renderer::Renderer()
{
}

// =====================================
// CALLBACK SCROLL
// =====================================

void Renderer::scrollCallback(
    GLFWwindow* window,
    double xoffset,
    double yoffset)
{
    Renderer* renderer =
        static_cast<Renderer*>(
            glfwGetWindowUserPointer(window)
            );

    if (renderer)
    {
        renderer->camera.zoom(
            (float)yoffset * 0.7f
        );
    }
}

// =====================================
// INICIALIZACION OPENGL
// =====================================

void Renderer::init(int width, int height)
{
    if (height <= 0)
        height = 1;

    glViewport(0, 0, width, height);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glClearColor(
        0.5f, 0.8f, 1.0f, 1.0f
    );

    float aspect = (float)width / height;

    float nearPlane = 1.0f;
    float farPlane = 100.0f;
    float fov = 45.0f;

    float top =
        std::tan(fov * PI / 360.0f)
        * nearPlane;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glFrustum(
        -top * aspect,
        top * aspect,
        -top,
        top,
        nearPlane,
        farPlane
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // RUTA DE TU MODELO
    cargarTaza(
        "C:/Users/janus/source/repos/"
        "PARCIAL-ULTIMO-CORTE-/YoutubeOpenGL/"
        "Models/scene.gltf"
    );

    lastTime = (float)glfwGetTime();
}

// =====================================
// DIBUJAR ESCENA
// =====================================

void Renderer::display()
{
    glClear(
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // ROTACION CAMARA
    glRotatef(
        camera.getRotationX(),
        1.0f, 0.0f, 0.0f
    );

    glRotatef(
        camera.getRotationY(),
        0.0f, 1.0f, 0.0f
    );

    // MOVIMIENTO CAMARA
    glTranslatef(
        camera.getX(),
        -camera.getY(),
        camera.getZ()
    );

    // DIBUJAR CURVA
    drawBezierCurve();

    // DELTA TIME
    float currentTime =
        (float)glfwGetTime();

    float deltaTime =
        currentTime - lastTime;

    lastTime = currentTime;

    if (deltaTime < 0.0f)
        deltaTime = 0.0f;

    if (deltaTime > 0.1f)
        deltaTime = 0.1f;

    // POSICION SOBRE BEZIER
    float x, y, z;

    calcularBezier(bezierT, x, y, z);

    // DERIVADA PARA ROTACION
    float t = bezierT;
    float u = 1.0f - t;

    float dx =
        3.0f * u * u * 4.0f +
        6.0f * u * t * 8.0f +
        3.0f * t * t * 4.0f;

    float dz =
        3.0f * u * u * 13.0f +
        6.0f * u * t * (-16.0f) +
        3.0f * t * t * 13.0f;

    float angle =
        std::atan2(dx, dz)
        * 180.0f / PI;

    // DIBUJAR MODELO SOBRE CURVA
    glPushMatrix();

    glTranslatef(x, y, z);

    glRotatef(
        angle,
        0.0f, 1.0f, 0.0f
    );

    glScalef(
        1.0f, 1.0f, 1.0f
    );

    dibujarModeloGLTF();

    glPopMatrix();

    // ANIMACION AUTOMATICA
    bezierT += deltaTime * 0.03f;

    if (bezierT >= 1.0f)
        bezierT = 0.0f;
}

// =====================================
// CONTROLES
// =====================================

void Renderer::processInput(GLFWwindow* window)
{
    // CERRAR CON ESC
    if (glfwGetKey(
        window, GLFW_KEY_ESCAPE
    ) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(
            window, GLFW_TRUE
        );
    }

    float sideSpeed = 0.01f;
    float verticalSpeed = 0.01f;
    float mouseSensitivity = 0.1f;

    // IZQUIERDA
    if (glfwGetKey(
        window, GLFW_KEY_D
    ) == GLFW_PRESS)
    {
        camera.moveLeft(sideSpeed);
    }

    // DERECHA
    if (glfwGetKey(
        window, GLFW_KEY_A
    ) == GLFW_PRESS)
    {
        camera.moveRight(sideSpeed);
    }

    // ARRIBA
    if (glfwGetKey(
        window, GLFW_KEY_W
    ) == GLFW_PRESS)
    {
        camera.moveUp(verticalSpeed);
    }

    // ABAJO
    if (glfwGetKey(
        window, GLFW_KEY_S
    ) == GLFW_PRESS)
    {
        camera.moveDown(verticalSpeed);
    }

    // ROTACION CON MOUSE
    if (glfwGetMouseButton(
        window, GLFW_MOUSE_BUTTON_LEFT
    ) == GLFW_PRESS)
    {
        double xpos, ypos;

        glfwGetCursorPos(
            window, &xpos, &ypos
        );

        if (firstMouse)
        {
            lastMouseX = xpos;
            lastMouseY = ypos;
            firstMouse = false;
        }

        float deltaX =
            (float)(xpos - lastMouseX)
            * mouseSensitivity;

        float deltaY =
            (float)(ypos - lastMouseY)
            * mouseSensitivity;

        lastMouseX = xpos;
        lastMouseY = ypos;

        camera.rotate(deltaX, deltaY);
    }
    else
    {
        firstMouse = true;
    }
}

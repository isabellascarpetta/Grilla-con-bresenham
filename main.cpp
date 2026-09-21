#include <iostream>
#include <vector>
#include <cmath>
#include <cstddef>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "ShaderClass.h"
#include "Origin.h"


// Estructura que representa cada vértice del cuadrado
struct VertexSquare {
    GLfloat pos[3];  
    GLfloat color[4]; 
};

// Buffers de OpenGL para la GPU
static GLuint g_VAO_sq = 0, g_VBO_sq = 0, g_EBO_sq = 0;

// Almacenamiento en CPU para la geometría de la grilla
static std::vector<VertexSquare> g_gridVerts; // Vértices de los cuadrados
static std::vector<GLuint> g_gridIndices;     // Índices para formar los triángulos

// Dimensiones de la grilla
static int g_gridCols = 80; // Número de columnas
static int g_gridRows = 80; // Número de filas

// Colores en formato RGBA [0.0 a 1.0]
static GLfloat g_baseColor[4] = { 0.2f, 0.2f, 0.2f, 1.0f }; // Gris oscuro para la grilla vacía
static GLfloat g_lineColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f }; // Rojo brillante para las líneas

static GLsizei g_squareIndexCount = 0;

// Variables para calcular las dimensiones de las celdas en NDC
static float g_startX = -1.0f, g_startY = -1.0f;
static float g_cellW = 0.0f, g_cellH = 0.0f;

// Variables para controlar la interactividad con el Mouse (Paint)
static bool g_isDrawing = false;
static int g_startCellX = -1, g_startCellY = -1;
static int g_currentCellX = -1, g_currentCellY = -1;

// Guardamos todas las líneas dibujadas previamente para que no se borren
struct Line { int x0, y0, x1, y1; };
static std::vector<Line> g_userLines;

// Crea y envía los datos de la grilla a la memoria de la tarjeta gráfica (GPU)
static void CreateSquareVAO(GLuint& VAO, GLuint& VBO, GLuint& EBO,
    const VertexSquare* verts, size_t vertCount,
    const GLuint* indices, size_t indexCount)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Subir Vértices
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertCount * sizeof(VertexSquare), verts, GL_STATIC_DRAW);

    // Subir Índices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(GLuint), indices, GL_STATIC_DRAW);

    GLsizei strideSquare = static_cast<GLsizei>(sizeof(VertexSquare));

    // Atributo 0: Posición (X, Y, Z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, strideSquare, (GLvoid*)offsetof(VertexSquare, pos));
    glEnableVertexAttribArray(0);

    // Atributo 1: Color (R, G, B, A)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, strideSquare, (GLvoid*)offsetof(VertexSquare, color));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Genera los rectángulos (cuadrados) que componen la grilla en coordenadas NDC (-1 a 1)
static void CreateGridSquares(std::vector<VertexSquare>& outVerts, std::vector<GLuint>& outIndices,
    int cols, int rows, float startX, float startY, float cellW, float cellH,
    const GLfloat color[4])
{
    outVerts.clear();
    outIndices.clear();
    outVerts.reserve(static_cast<size_t>(cols) * rows * 4);
    outIndices.reserve(static_cast<size_t>(cols) * rows * 6);

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            float cx = startX + (x + 0.5f) * cellW;
            float cy = startY + (y + 0.5f) * cellH;
            float halfW = cellW * 0.5f;
            float halfH = cellH * 0.5f;

            // 4 esquinas de la celda 
            VertexSquare v0 = { {cx + halfW, cy + halfH, 0.0f}, {color[0], color[1], color[2], color[3]} }; // Arriba-Derecha
            VertexSquare v1 = { {cx + halfW, cy - halfH, 0.0f}, {color[0], color[1], color[2], color[3]} }; // Abajo-Derecha
            VertexSquare v2 = { {cx - halfW, cy - halfH, 0.0f}, {color[0], color[1], color[2], color[3]} }; // Abajo-Izquierda
            VertexSquare v3 = { {cx - halfW, cy + halfH, 0.0f}, {color[0], color[1], color[2], color[3]} }; // Arriba-Izquierda

            GLuint base = static_cast<GLuint>(outVerts.size());
            outVerts.push_back(v0);
            outVerts.push_back(v1);
            outVerts.push_back(v2);
            outVerts.push_back(v3);

            // Dos triángulos por cada celda/cuadrado
            outIndices.push_back(base + 0);
            outIndices.push_back(base + 1);
            outIndices.push_back(base + 2);
            outIndices.push_back(base + 0);
            outIndices.push_back(base + 2);
            outIndices.push_back(base + 3);
        }
    }
}

// Ajusta el tamaño de las celdas según el área visible
static void CreateGridSquaresForViewport(std::vector<VertexSquare>& outVerts, std::vector<GLuint>& outIndices,
    int cols, int rows, const GLfloat color[4],
    float& outStartX, float& outStartY, float& outCellW, float& outCellH)
{
    const float ndcWidth = 2.0f;  // El espacio NDC de OpenGL va de -1.0 a +1.0 (Ancho = 2.0)
    const float ndcHeight = 2.0f; // Alto = 2.0

    outCellW = ndcWidth / static_cast<float>(cols);
    outCellH = ndcHeight / static_cast<float>(rows);

    outStartX = -1.0f;
    outStartY = -1.0f;

    CreateGridSquares(outVerts, outIndices, cols, rows, outStartX, outStartY, outCellW, outCellH, color);
}

// Colorea una celda individual y la línea 
static void PaintLineBresenham(std::vector<VertexSquare>& verts, int cols, int rows,
    int x0, int y0, int x1, int y1, const GLfloat color[4])
{
    // Función para cambiar el color de los 4 vértices de una celda (cx, cy)
    auto setCellColor = [&](int cx, int cy) {
        if (cx < 0 || cx >= cols || cy < 0 || cy >= rows) return;
        size_t cellIndex = static_cast<size_t>(cy) * cols + static_cast<size_t>(cx);
        size_t base = cellIndex * 4;
        for (size_t i = 0; i < 4; ++i) {
            verts[base + i].color[0] = color[0];
            verts[base + i].color[1] = color[1];
            verts[base + i].color[2] = color[2];
            verts[base + i].color[3] = color[3];
        }
        };

    //ALGORITMO DE BRESENHAM PARA PINTADO DE LINEAS
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        setCellColor(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Reconstruye la grilla y vuelve a pintar todas las líneas acumuladas
static void RecreateGridForViewport(GLFWwindow* window)
{
    CreateGridSquaresForViewport(g_gridVerts, g_gridIndices, g_gridCols, g_gridRows,
        g_baseColor, g_startX, g_startY, g_cellW, g_cellH);

    // 1. Repintar todas las líneas anteriores guardadas por el usuario
    for (const auto& line : g_userLines) {
        PaintLineBresenham(g_gridVerts, g_gridCols, g_gridRows, line.x0, line.y0, line.x1, line.y1, g_lineColor);
    }

    // 2. Si el usuario está arrastrando el mouse en tiempo real, se pinta la linea
    if (g_isDrawing) {
        PaintLineBresenham(g_gridVerts, g_gridCols, g_gridRows, g_startCellX, g_startCellY, g_currentCellX, g_currentCellY, g_lineColor);
    }

    // Liberar buffers viejos de la GPU y subir los nuevos
    if (g_VAO_sq) {
        glDeleteVertexArrays(1, &g_VAO_sq);
        glDeleteBuffers(1, &g_VBO_sq);
        glDeleteBuffers(1, &g_EBO_sq);
        g_VAO_sq = g_VBO_sq = g_EBO_sq = 0;
    }

    CreateSquareVAO(g_VAO_sq, g_VBO_sq, g_EBO_sq, g_gridVerts.data(), g_gridVerts.size(),
        g_gridIndices.data(), g_gridIndices.size());
    g_squareIndexCount = static_cast<GLsizei>(g_gridIndices.size());
}


// Ajusta el viewport cuando la ventana se escala
static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    RecreateGridForViewport(window);
}

// para convertir las coordenadas del cursor del mouse en píxeles de pantalla a celdas (Columna, Fila)
static void ScreenToGridCoords(GLFWwindow* window, double xpos, double ypos, int& outCellX, int& outCellY)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    if (width <= 0 || height <= 0) return;

    // Normalizar a rango [0.0, 1.0]
    double normX = xpos / static_cast<double>(width);
    double normY = 1.0 - (ypos / static_cast<double>(height)); // Invertir el eje Y

    outCellX = static_cast<int>(normX * g_gridCols);
    outCellY = static_cast<int>(normY * g_gridRows);

    // Clampear límites para evitar errores fuera de rango
    outCellX = std::max(0, std::min(g_gridCols - 1, outCellX));
    outCellY = std::max(0, std::min(g_gridRows - 1, outCellY));
}

// clic de mouse (Presionar / Soltar)
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        if (action == GLFW_PRESS) {
            g_isDrawing = true;
            ScreenToGridCoords(window, xpos, ypos, g_startCellX, g_startCellY);
            g_currentCellX = g_startCellX;
            g_currentCellY = g_startCellY;
            RecreateGridForViewport(window);
        }
        else if (action == GLFW_RELEASE && g_isDrawing) {
            g_isDrawing = false;
            ScreenToGridCoords(window, xpos, ypos, g_currentCellX, g_currentCellY);

            // Guardar la línea dibujada
            g_userLines.push_back({ g_startCellX, g_startCellY, g_currentCellX, g_currentCellY });
            RecreateGridForViewport(window);
        }
    }
}

// Para pintar la linea con el mouse cuando se arrastra
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (g_isDrawing) {
        int newCellX, newCellY;
        ScreenToGridCoords(window, xpos, ypos, newCellX, newCellY);
        if (newCellX != g_currentCellX || newCellY != g_currentCellY) {
            g_currentCellX = newCellX;
            g_currentCellY = newCellY;
            RecreateGridForViewport(window);
        }
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Paint de Lineas - Algoritmo Bresenham", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Error al crear la ventana de GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Error al inicializar GLAD" << std::endl;
        return -1;
    }

    // Configuración de callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Compilar e inicializar shaders y ejes de referencia
    ShaderClass shader("vertex.vs", "fragment.fs");
    Origin origin;

    // Crear grilla inicial
    RecreateGridForViewport(window);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Bucle principal (Game Loop)
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        // Activar Shader de la grilla y renderizar
        shader.Activate(shader.ID);
        glBindVertexArray(g_VAO_sq);
        glDrawElements(GL_TRIANGLES, g_squareIndexCount, GL_UNSIGNED_INT, 0);

        // Dibujar ejes de origen si existen
       // origin.draw();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Limpieza de recursos al cerrar
    shader.Delete(shader.ID);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
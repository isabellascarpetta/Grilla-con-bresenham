#include <iostream>
#include <vector>
#include <queue>
#include <utility>
#include <cmath>
#include <cstddef>
#include <algorithm>

// Librerías de OpenGL y gestión de ventanas
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Librerías de Dear ImGui para la interfaz gráfica (GUI)
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Clases propias del proyecto para Shaders y Ejes de Origen
#include "ShaderClass.h"
#include "Origin.h"

// ============================================================================
// ESTRUCTURAS DE DATOS
// ============================================================================

// Estructura que representa cada vértice de un cuadrado (celda) en la GPU
struct VertexSquare {
    GLfloat pos[3];   // Coordenadas X, Y, Z en espacio normalizado (NDC)
    GLfloat color[4]; // Color en formato RGBA [0.0 a 1.0]
};

// Estructura auxiliar para manejar colores RGBA en la lógica de CPU
struct ColorRGBA {
    float r, g, b, a;

    // Operador de comparación con tolerancia para el algoritmo de relleno (Flood Fill)
    bool operator==(const ColorRGBA& other) const {
        return std::abs(r - other.r) < 0.01f &&
            std::abs(g - other.g) < 0.01f &&
            std::abs(b - other.b) < 0.01f &&
            std::abs(a - other.a) < 0.01f;
    }

    bool operator!=(const ColorRGBA& other) const {
        return !(*this == other);
    }
};

// ============================================================================
// VARIABLES GLOBALES DE LA GRILLA Y OPENGL
// ============================================================================

// Identificadores de los buffers de OpenGL en la tarjeta gráfica (VRAM)
static GLuint g_VAO_sq = 0, g_VBO_sq = 0, g_EBO_sq = 0;

// Geometría de vértices e índices que se envían a la GPU
static std::vector<VertexSquare> g_gridVerts;
static std::vector<GLuint> g_gridIndices;
static GLsizei g_squareIndexCount = 0;

// Dimensiones de la grilla (80 columnas x 80 filas)
static int g_gridCols = 80;
static int g_gridRows = 80;

// Matriz linealizada en CPU que almacena el color permanente de cada celda del lienzo
static std::vector<ColorRGBA> g_canvasColors;

// Coordenadas de inicio y dimensiones de cada celda en espacio NDC (-1.0 a 1.0)
static float g_startX = -1.0f, g_startY = -1.0f;
static float g_cellW = 0.0f, g_cellH = 0.0f;

// Color de fondo por defecto del lienzo (Gris oscuro)
static ColorRGBA g_baseColor = { 0.12f, 0.12f, 0.15f, 1.0f };

// Color activo seleccionado por el usuario desde ImGui (inicia en Rojo)
static float g_activeColor[4] = { 1.0f, 0.2f, 0.2f, 1.0f };

// ============================================================================
// HERRAMIENTAS DE DIBUJO E INTERACCIÓN CON EL MOUSE
// ============================================================================

// Enumeración con todas las herramientas solicitadas para el Paint
enum DrawMode {
    MODE_LINE = 0,     // Línea recta (Bresenham)
    MODE_CIRCLE = 1,   // Círculo (Bresenham / Punto Medio)
    MODE_FREEFORM = 2, // Dibujo libre a mano alzada
    MODE_TRIANGLE = 3, // Triángulo en la grilla
    MODE_SQUARE = 4,   // Cuadrado en la grilla
    MODE_FILL = 5      // Bote de pintura (Relleno por inundación)
};

// Herramienta seleccionada actualmente
static DrawMode g_currentMode = MODE_LINE;

// Variables de estado para controlar el clic y arrastre del ratón
static bool g_isDrawing = false;
static int g_startCellX = -1, g_startCellY = -1;
static int g_currentCellX = -1, g_currentCellY = -1;

// ============================================================================
// FUNCIONES DE PINTADO EN EL BUFFER DE CELDAS (CPU)
// ============================================================================

// Asigna un color a una celda específica (cx, cy) dentro de un buffer dado
static void SetBufferCellColor(std::vector<ColorRGBA>& buffer, int cx, int cy, const ColorRGBA& color) {
    // Validar que las coordenadas estén dentro de los límites de la grilla
    if (cx < 0 || cx >= g_gridCols || cy < 0 || cy >= g_gridRows) return;
    size_t index = static_cast<size_t>(cy) * g_gridCols + static_cast<size_t>(cx);
    buffer[index] = color;
}

// Obtiene el color actual de una celda específica (cx, cy)
static ColorRGBA GetBufferCellColor(const std::vector<ColorRGBA>& buffer, int cx, int cy) {
    if (cx < 0 || cx >= g_gridCols || cy < 0 || cy >= g_gridRows) return g_baseColor;
    size_t index = static_cast<size_t>(cy) * g_gridCols + static_cast<size_t>(cx);
    return buffer[index];
}

// 1. ALGORITMO DE BRESENHAM PARA LÍNEAS
static void PaintLineBresenham(std::vector<ColorRGBA>& buffer, int x0, int y0, int x1, int y1, const ColorRGBA& color) {
    int dx = std::abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy; // Acumulador de margen de error

    while (true) {
        SetBufferCellColor(buffer, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// 2. ALGORITMO DE BRESENHAM (PUNTO MEDIO) PARA CÍRCULOS
static void PaintCircleBresenham(std::vector<ColorRGBA>& buffer, int xc, int yc, int r, const ColorRGBA& color) {
    if (r <= 0) {
        SetBufferCellColor(buffer, xc, yc, color);
        return;
    }

    // Función lambda para pintar los 8 octantes simétricos del círculo
    auto plot8Points = [&](int x, int y) {
        SetBufferCellColor(buffer, xc + x, yc + y, color);
        SetBufferCellColor(buffer, xc - x, yc + y, color);
        SetBufferCellColor(buffer, xc + x, yc - y, color);
        SetBufferCellColor(buffer, xc - x, yc - y, color);
        SetBufferCellColor(buffer, xc + y, yc + x, color);
        SetBufferCellColor(buffer, xc - y, yc + x, color);
        SetBufferCellColor(buffer, xc + y, yc - x, color);
        SetBufferCellColor(buffer, xc - y, yc - x, color);
        };

    int x = 0;
    int y = r;
    int d = 3 - 2 * r; // Parámetro de decisión inicial
    plot8Points(x, y);

    while (y >= x) {
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        }
        else {
            d = d + 4 * x + 6;
        }
        plot8Points(x, y);
    }
}

// 3. ALGORITMO PARA DIBUJAR CUADRADOS EN LA GRILLA
static void PaintSquareBresenham(std::vector<ColorRGBA>& buffer, int x0, int y0, int x1, int y1, const ColorRGBA& color) {
    // Calcular el lado máximo para mantener la proporción 1:1 de un cuadrado
    int side = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    int dirX = (x1 >= x0) ? 1 : -1;
    int dirY = (y1 >= y0) ? 1 : -1;
    int endX = x0 + dirX * side;
    int endY = y0 + dirY * side;

    // Trazar los 4 lados del cuadrado usando líneas de Bresenham
    PaintLineBresenham(buffer, x0, y0, endX, y0, color);     // Lado horizontal inicial
    PaintLineBresenham(buffer, endX, y0, endX, endY, color); // Lado vertical opuesto
    PaintLineBresenham(buffer, endX, endY, x0, endY, color); // Lado horizontal opuesto
    PaintLineBresenham(buffer, x0, endY, x0, y0, color);     // Lado vertical inicial
}

// 4. ALGORITMO PARA DIBUJAR TRIÁNGULOS EN LA GRILLA
static void PaintTriangleBresenham(std::vector<ColorRGBA>& buffer, int x0, int y0, int x1, int y1, const ColorRGBA& color) {
    // Definir los 3 vértices de un triángulo isósceles dentro del área arrastrada:
    // Vértice superior centrado en X, y dos vértices en la base inferior
    int topX = (x0 + x1) / 2;
    int topY = y0;
    int leftX = x0;
    int leftY = y1;
    int rightX = x1;
    int rightY = y1;

    // Unir los 3 vértices mediante líneas de Bresenham
    PaintLineBresenham(buffer, topX, topY, leftX, leftY, color);
    PaintLineBresenham(buffer, leftX, leftY, rightX, rightY, color);
    PaintLineBresenham(buffer, rightX, rightY, topX, topY, color);
}

// 5. ALGORITMO DE RELLENO POR INUNDACIÓN (FLOOD FILL ITERATIVO - BFS)
static void FloodFill(std::vector<ColorRGBA>& buffer, int startX, int startY, const ColorRGBA& fillColor) {
    if (startX < 0 || startX >= g_gridCols || startY < 0 || startY >= g_gridRows) return;

    // Color original de la celda donde el usuario hizo clic
    ColorRGBA targetColor = GetBufferCellColor(buffer, startX, startY);

    // Si el color destino ya es igual al color de relleno, no hacer nada
    if (targetColor == fillColor) return;

    // Cola para recorrido en anchura (evita desbordamiento de pila frente a recursión)
    std::queue<std::pair<int, int>> q;
    std::vector<bool> visited(static_cast<size_t>(g_gridCols) * g_gridRows, false);

    q.push({ startX, startY });
    visited[static_cast<size_t>(startY) * g_gridCols + startX] = true;

    // Desplazamientos para conectividad de 4 vecinos (Arriba, Abajo, Izquierda, Derecha)
    // Se usa 4-conectividad para que el relleno no se escape por bordes diagonales de Bresenham
    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!q.empty()) {
        std::pair<int, int> curr = q.front();
        q.pop();

        int cx = curr.first;
        int cy = curr.second;

        // Pintar la celda actual con el nuevo color
        SetBufferCellColor(buffer, cx, cy, fillColor);

        // Explorar los 4 vecinos ortogonales
        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (nx >= 0 && nx < g_gridCols && ny >= 0 && ny < g_gridRows) {
                size_t idx = static_cast<size_t>(ny) * g_gridCols + static_cast<size_t>(nx);
                if (!visited[idx] && GetBufferCellColor(buffer, nx, ny) == targetColor) {
                    visited[idx] = true;
                    q.push({ nx, ny });
                }
            }
        }
    }
}

// ============================================================================
// GESTIÓN DE GEOMETRÍA Y BUFFERS DE OPENGL (GPU)
// ============================================================================

// Crea o actualiza los buffers VAO, VBO y EBO en la tarjeta gráfica
static void UploadGridToGPU() {
    if (g_VAO_sq == 0) {
        glGenVertexArrays(1, &g_VAO_sq);
        glGenBuffers(1, &g_VBO_sq);
        glGenBuffers(1, &g_EBO_sq);
    }

    glBindVertexArray(g_VAO_sq);

    // Subir arreglo de vértices (posiciones y colores)
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO_sq);
    glBufferData(GL_ARRAY_BUFFER, g_gridVerts.size() * sizeof(VertexSquare), g_gridVerts.data(), GL_DYNAMIC_DRAW);

    // Subir arreglo de índices de triángulos
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_EBO_sq);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, g_gridIndices.size() * sizeof(GLuint), g_gridIndices.data(), GL_STATIC_DRAW);

    GLsizei stride = static_cast<GLsizei>(sizeof(VertexSquare));

    // Atributo 0: Posición (X, Y, Z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid*)offsetof(VertexSquare, pos));
    glEnableVertexAttribArray(0);

    // Atributo 1: Color (R, G, B, A)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*)offsetof(VertexSquare, color));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Reconstruye la malla visual combinando el lienzo guardado + vista previa en tiempo real
static void UpdateGridVisuals() {
    // Copiar el estado permanente del lienzo a un buffer temporal de renderizado
    std::vector<ColorRGBA> displayBuffer = g_canvasColors;
    ColorRGBA currentCol = { g_activeColor[0], g_activeColor[1], g_activeColor[2], g_activeColor[3] };

    // Si el usuario está arrastrando el mouse, dibujar la previsualización encima sin alterar el lienzo aún
    if (g_isDrawing) {
        if (g_currentMode == MODE_LINE) {
            PaintLineBresenham(displayBuffer, g_startCellX, g_startCellY, g_currentCellX, g_currentCellY, currentCol);
        }
        else if (g_currentMode == MODE_CIRCLE) {
            int dx = g_currentCellX - g_startCellX;
            int dy = g_currentCellY - g_startCellY;
            int radius = static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy)));
            PaintCircleBresenham(displayBuffer, g_startCellX, g_startCellY, radius, currentCol);
        }
        else if (g_currentMode == MODE_SQUARE) {
            PaintSquareBresenham(displayBuffer, g_startCellX, g_startCellY, g_currentCellX, g_currentCellY, currentCol);
        }
        else if (g_currentMode == MODE_TRIANGLE) {
            PaintTriangleBresenham(displayBuffer, g_startCellX, g_startCellY, g_currentCellX, g_currentCellY, currentCol);
        }
    }

    // Regenerar los vértices e índices de los 6,400 cuadrados de la grilla
    g_gridVerts.clear();
    g_gridIndices.clear();
    g_gridVerts.reserve(static_cast<size_t>(g_gridCols) * g_gridRows * 4);
    g_gridIndices.reserve(static_cast<size_t>(g_gridCols) * g_gridRows * 6);

    g_cellW = 2.0f / static_cast<float>(g_gridCols);
    g_cellH = 2.0f / static_cast<float>(g_gridRows);
    g_startX = -1.0f;
    g_startY = -1.0f;

    for (int y = 0; y < g_gridRows; ++y) {
        for (int x = 0; x < g_gridCols; ++x) {
            float cx = g_startX + (x + 0.5f) * g_cellW;
            float cy = g_startY + (y + 0.5f) * g_cellH;
            float halfW = g_cellW * 0.5f;
            float halfH = g_cellH * 0.5f;

            // Obtener el color correspondiente de displayBuffer
            ColorRGBA c = displayBuffer[static_cast<size_t>(y) * g_gridCols + static_cast<size_t>(x)];

            // Definir las 4 esquinas del cuadrado
            VertexSquare v0 = { { cx + halfW, cy + halfH, 0.0f },{ c.r, c.g, c.b, c.a } };
            VertexSquare v1 = { { cx + halfW, cy - halfH, 0.0f },{ c.r, c.g, c.b, c.a } };
            VertexSquare v2 = { { cx - halfW, cy - halfH, 0.0f },{ c.r, c.g, c.b, c.a } };
            VertexSquare v3 = { { cx - halfW, cy + halfH, 0.0f },{ c.r, c.g, c.b, c.a } };

            GLuint base = static_cast<GLuint>(g_gridVerts.size());
            g_gridVerts.push_back(v0);
            g_gridVerts.push_back(v1);
            g_gridVerts.push_back(v2);
            g_gridVerts.push_back(v3);

            // Formar 2 triángulos por celda
            g_gridIndices.push_back(base + 0);
            g_gridIndices.push_back(base + 1);
            g_gridIndices.push_back(base + 2);
            g_gridIndices.push_back(base + 0);
            g_gridIndices.push_back(base + 2);
            g_gridIndices.push_back(base + 3);
        }
    }

    g_squareIndexCount = static_cast<GLsizei>(g_gridIndices.size());
    UploadGridToGPU();
}

// ============================================================================
// CALLBACKS DE GLFW (VENTANA, RATÓN Y TECLADO)
// ============================================================================

// Ajusta el área de renderizado cuando se redimensiona la ventana
static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
    UpdateGridVisuals();
}

// Convierte coordenadas de píxeles de pantalla a índices de celda (Columna, Fila)
static void ScreenToGridCoords(GLFWwindow* window, double xpos, double ypos, int& outCellX, int& outCellY) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    if (width <= 0 || height <= 0) return;

    double normX = xpos / static_cast<double>(width);
    double normY = 1.0 - (ypos / static_cast<double>(height)); // Invertir eje Y para OpenGL

    outCellX = static_cast<int>(normX * g_gridCols);
    outCellY = static_cast<int>(normY * g_gridRows);

    // Asegurar que las coordenadas estén dentro de [0, 79]
    outCellX = std::max(0, std::min(g_gridCols - 1, outCellX));
    outCellY = std::max(0, std::min(g_gridRows - 1, outCellY));
}

// Maneja los clics del mouse (Presionar y Soltar)
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    // IMPORTANTE: Si el cursor está sobre la ventana de Dear ImGui, ignorar el clic en la grilla
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        int cellX, cellY;
        ScreenToGridCoords(window, xpos, ypos, cellX, cellY);

        ColorRGBA currentCol = { g_activeColor[0], g_activeColor[1], g_activeColor[2], g_activeColor[3] };

        if (action == GLFW_PRESS) {
            // Si el modo es Rellenar, ejecutar Flood Fill inmediatamente al hacer clic
            if (g_currentMode == MODE_FILL) {
                FloodFill(g_canvasColors, cellX, cellY, currentCol);
                UpdateGridVisuals();
                return;
            }

            // Iniciar trazado para el resto de herramientas
            g_isDrawing = true;
            g_startCellX = cellX;
            g_startCellY = cellY;
            g_currentCellX = cellX;
            g_currentCellY = cellY;

            // Si es dibujo libre, pintar inmediatamente la primera celda tocada
            if (g_currentMode == MODE_FREEFORM) {
                SetBufferCellColor(g_canvasColors, cellX, cellY, currentCol);
            }
            UpdateGridVisuals();
        }
        else if (action == GLFW_RELEASE && g_isDrawing) {
            g_isDrawing = false;
            g_currentCellX = cellX;
            g_currentCellY = cellY;

            // Guardar definitivamente la figura trazada en g_canvasColors
            if (g_currentMode == MODE_LINE) {
                PaintLineBresenham(g_canvasColors, g_startCellX, g_startCellY, g_currentCellX, g_currentCellY, currentCol);
            }
            else if (g_currentMode == MODE_CIRCLE) {
                int dx = g_currentCellX - g_startCellX;
                int dy = g_currentCellY - g_startCellY;
                int r = static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy)));
                PaintCircleBresenham(g_canvasColors, g_startCellX, g_startCellY, r, currentCol);
            }
            else if (g_currentMode == MODE_SQUARE) {
                PaintSquareBresenham(g_canvasColors, g_startCellX, g_startCellY, g_currentCellX, g_currentCellY, currentCol);
            }
            else if (g_currentMode == MODE_TRIANGLE) {
                PaintTriangleBresenham(g_canvasColors, g_startCellX, g_startCellY, g_currentCellX, g_currentCellY, currentCol);
            }

            UpdateGridVisuals();
        }
    }
}

// Maneja el movimiento continuo del ratón mientras se mantiene presionado el botón
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    // Si ImGui tiene el foco del ratón, no dibujar en el lienzo
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (g_isDrawing) {
        int newCellX, newCellY;
        ScreenToGridCoords(window, xpos, ypos, newCellX, newCellY);

        if (newCellX != g_currentCellX || newCellY != g_currentCellY) {
            // En modo libre, conectar el punto anterior con el nuevo usando Bresenham para evitar huecos
            if (g_currentMode == MODE_FREEFORM) {
                ColorRGBA currentCol = { g_activeColor[0], g_activeColor[1], g_activeColor[2], g_activeColor[3] };
                PaintLineBresenham(g_canvasColors, g_currentCellX, g_currentCellY, newCellX, newCellY, currentCol);
            }

            g_currentCellX = newCellX;
            g_currentCellY = newCellY;
            UpdateGridVisuals();
        }
    }
}

// Atajos rápidos de teclado
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    (void)mods;
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) g_currentMode = MODE_LINE;
        else if (key == GLFW_KEY_2) g_currentMode = MODE_CIRCLE;
        else if (key == GLFW_KEY_3) g_currentMode = MODE_FREEFORM;
        else if (key == GLFW_KEY_4) g_currentMode = MODE_TRIANGLE;
        else if (key == GLFW_KEY_5) g_currentMode = MODE_SQUARE;
        else if (key == GLFW_KEY_6) g_currentMode = MODE_FILL;
        else if (key == GLFW_KEY_R) {
            // Limpiar todo el lienzo rellenándolo con el color base
            std::fill(g_canvasColors.begin(), g_canvasColors.end(), g_baseColor);
            UpdateGridVisuals();
        }
    }
}

// ============================================================================
// FUNCIÓN PRINCIPAL (MAIN)
// ============================================================================

int main() {
    // 1. Inicializar GLFW y configurar versión de OpenGL 3.3 Core
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1000, 800, "Paint OpenGL + Dear ImGui (Bresenham)", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Error al crear la ventana de GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Inicializar GLAD para cargar punteros de funciones de OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Error al inicializar GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Registrar callbacks ANTES de inicializar ImGui para que ambos funcionen en cadena
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetKeyCallback(window, key_callback);

    // 2. Inicializar contexto y backends de Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark(); // Tema oscuro moderno

    // Vincular ImGui con GLFW y OpenGL 3.3
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Compilar shaders propios e inicializar lienzo
    ShaderClass shader("vertex.vs", "fragment.fs");
    Origin origin;
    (void)origin;

    // Inicializar todas las celdas del lienzo con el color base
    g_canvasColors.resize(static_cast<size_t>(g_gridCols) * g_gridRows, g_baseColor);
    UpdateGridVisuals();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Bucle principal de renderizado (Game Loop)
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        // --- INICIO DEL FRAME DE DEAR IMGUI ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Construir ventana flotante de herramientas
        ImGui::Begin("Herramientas de Paint");
        ImGui::Text("Selecciona una herramienta:");
        ImGui::Separator();

        // Botones de selección de herramienta (RadioButtons)
        if (ImGui::RadioButton("Linea (Bresenham) [1]", g_currentMode == MODE_LINE)) {
            g_currentMode = MODE_LINE;
        }
        if (ImGui::RadioButton("Circulo (Bresenham) [2]", g_currentMode == MODE_CIRCLE)) {
            g_currentMode = MODE_CIRCLE;
        }
        if (ImGui::RadioButton("Forma Libre (Lapiz) [3]", g_currentMode == MODE_FREEFORM)) {
            g_currentMode = MODE_FREEFORM;
        }
        if (ImGui::RadioButton("Triangulo [4]", g_currentMode == MODE_TRIANGLE)) {
            g_currentMode = MODE_TRIANGLE;
        }
        if (ImGui::RadioButton("Cuadrado [5]", g_currentMode == MODE_SQUARE)) {
            g_currentMode = MODE_SQUARE;
        }
        if (ImGui::RadioButton("Rellenar Color (Bote) [6]", g_currentMode == MODE_FILL)) {
            g_currentMode = MODE_FILL;
        }

        ImGui::Separator();
        ImGui::Text("Color de dibujo / relleno:");
        // Selector interactivo de color RGBA
        ImGui::ColorEdit4("Color", g_activeColor);

        ImGui::Separator();
        // Botón para reiniciar todo el lienzo
        if (ImGui::Button("Limpiar Lienzo [R]", ImVec2(180, 30))) {
            std::fill(g_canvasColors.begin(), g_canvasColors.end(), g_baseColor);
            UpdateGridVisuals();
        }
        ImGui::End();
        // --- FIN DEL FRAME DE DEAR IMGUI ---

        // 3. Dibujar la grilla en OpenGL
        shader.Activate(shader.ID);
        glBindVertexArray(g_VAO_sq);
        glDrawElements(GL_TRIANGLES, g_squareIndexCount, GL_UNSIGNED_INT, 0);

        // Renderizar la interfaz de Dear ImGui por encima de la grilla
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Intercambiar buffers y procesar eventos
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 4. Limpieza ordenada de recursos de ImGui, OpenGL y GLFW
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &g_VAO_sq);
    glDeleteBuffers(1, &g_VBO_sq);
    glDeleteBuffers(1, &g_EBO_sq);
    shader.Delete(shader.ID);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
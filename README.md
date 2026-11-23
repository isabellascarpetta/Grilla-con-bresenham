Nombre: Isabella Scarpetta Escobar
Codigo: 6014312

El sistema implementa una arquitectura híbrida CPU–GPU orientada a la rasterización en tiempo real 
sobre una cuadrícula discreta de 80x80 celdas (6,400 píxeles lógicos). 
La lógica geométrica y el almacenamiento de estado se procesan en la memoria principal (CPU), 
mientras que el renderizado masivo de celdas y la interfaz gráfica se delegan a la tarjeta gráfica (GPU) 
mediante OpenGL 3.3 Core y Dear ImGui. 

El proyecto fue desarrollado y compilado en Microsoft Visual Studio bajo arquitectura de 64 bits (x64).

Para el apartado gráfico se empleó OpenGL 3.3 Core Profile junto con GLAD para la carga de funciones modernas, GLFW 3 para la gestión de ventanas y eventos, y Dear ImGui para los controles visuales. Además, utiliza shaders personalizados en GLSL 330 Core (vertex.vs y fragment.fs).

Arquitectura del Proyecto

El sistema opera bajo un modelo híbrido CPU–GPU dividido en tres capas independientes:

1. Interfaz y Captura de Eventos

GLFW captura los movimientos del ratón y atajos del teclado, mientras Dear ImGui renderiza el menú flotante. El sistema consulta las banderas WantCaptureMouse y WantCaptureKeyboard para evitar pintar en el lienzo mientras se interactúa con los botones.

2. Lógica y Rasterización (CPU)

En la memoria principal se mantiene una matriz linealizada permanente (g_canvasColors) y un buffer temporal (displayBuffer). Sobre estas estructuras se calculan todos los trazos geométricos y rellenos antes de enviarlos a pantalla.

3. Renderizado Gráfico (GPU)

La cuadrícula lógica se transforma en una malla de coordenadas normalizadas (NDC) compuesta por 6,400 cuadrados (12,800 triángulos). Esta geometría se transfiere a la tarjeta gráfica mediante un VAO, un VBO dinámico y un EBO estático para dibujarse en una sola llamada con glDrawElements.

- Funcionamiento General

Al iniciar, el programa abre una ventana de 1000 X 800 píxeles y registra los callbacks de GLFW antes de inicializar Dear ImGui. Esto permite que la interfaz encadene los eventos del sistema operativo sin bloquear la lógica de dibujo.

Cuando el usuario selecciona una herramienta y arrastra el cursor, el trazo se dibuja primero en el buffer temporal para ofrecer una vista previa fluida sin alterar el dibujo base. Al soltar el clic izquierdo, la figura se graba definitivamente en el lienzo permanente y se actualiza en la GPU.

- Agrupación de Funciones (main.cpp)

El código fuente está organizado en cinco bloques funcionales:

* Gestión del Buffer en CPU: SetBufferCellColor y GetBufferCellColor traducen coordenadas 2D a índices lineales dentro del rango [0, 79], validando los límites de memoria antes de leer o escribir colores.

* Algoritmos de Rasterización y Relleno: PaintLineBresenham traza rectas usando aritmética entera; PaintCircleBresenham aplica el algoritmo del punto medio replicando un octante por simetría; PaintSquareBresenham y PaintTriangleBresenham unen vértices con líneas de Bresenham; y FloodFill ejecuta un relleno iterativo (BFS) con conectividad de 4 vecinos.

* Geometría y Memoria VRAM: UpdateGridVisuals combina el lienzo base con la vista previa activa para generar los 25,600 vértices de la malla, mientras UploadGridToGPU envía los datos actualizados a los buffers de OpenGL.

* Callbacks de Entrada: ScreenToGridCoords mapea píxeles de pantalla a celdas invirtiendo el eje Y; mouse_button_callback y cursor_position_callback gestionan el inicio, arrastre continuo y cierre de cada trazo; y key_callback habilita los atajos 1–6 y la tecla R para limpiar el lienzo.

* Ciclo Principal (main): Orquesta la inicialización de librerías, el bucle de renderizado por cuadro con los paneles de Dear ImGui y la liberación ordenada de recursos al cerrar la ventana.

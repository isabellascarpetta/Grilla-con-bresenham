#pragma once
#ifndef ORIGIN_H
#define ORIGIN_H

#include <glad/glad.h>

class Origin {
public:
    unsigned int VAO, VBO;

    Origin() {
        // Coordenadas de los ejes X (Rojo) y Y (Verde)
        float vertices[] = {
            // Posiciones XYZ      // Colores RGBA
            -1.0f,  0.0f,  0.0f,   1.0f, 0.0f, 0.0f, 1.0f, // Eje X (Izquierda)
             1.0f,  0.0f,  0.0f,   1.0f, 0.0f, 0.0f, 1.0f, // Eje X (Derecha)
             0.0f, -1.0f,  0.0f,   0.0f, 1.0f, 0.0f, 1.0f, // Eje Y (Abajo)
             0.0f,  1.0f,  0.0f,   0.0f, 1.0f, 0.0f, 1.0f  // Eje Y (Arriba)
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Atributo 0: Posición
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Atributo 1: Color
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void draw() {
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, 4);
        glBindVertexArray(0);
    }
};

#endif
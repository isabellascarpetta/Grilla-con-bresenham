#ifndef SHADER_CLASS_H
#define SHADER_CLASS_H

#include <glad/glad.h>
#include <iostream>

// Código del Vertex Shader integrado
const char* defaultVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

out vec4 ourColor;

void main() {
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
}
)";

// Código del Fragment Shader integrado
const char* defaultFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec4 ourColor;

void main() {
    FragColor = ourColor;
}
)";

class ShaderClass {
public:
    GLuint ID;

    ShaderClass(const char* vertexPath = nullptr, const char* fragmentPath = nullptr) {
        const char* vShaderCode = defaultVertexShaderSource;
        const char* fShaderCode = defaultFragmentShaderSource;

        // 1. Compilar Vertex Shader
        GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);

        // 2. Compilar Fragment Shader
        GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);

        // 3. Crear y enlazar programa
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void Activate(GLuint shaderID) {
        glUseProgram(shaderID);
    }

    void Delete(GLuint shaderID) {
        glDeleteProgram(shaderID);
    }
};

#endif
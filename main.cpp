#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <cstring>

static inline float clampTextureValue(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

struct Apple {
    glm::vec3 position;
    float size;
    float rpm;
    float currentAngle;
    glm::vec3 rotationAxis;
};

std::vector<Apple> g_Apples;

const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoords;

    out vec3 FragPos;
    out vec3 Normal;
    out vec2 TexCoords;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        TexCoords = aTexCoords;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    in vec3 FragPos;
    in vec3 Normal;
    in vec2 TexCoords;

    uniform sampler2D texture_diffuse;
    uniform vec3 lightPos;
    uniform vec3 viewPos;
    uniform float shininess;

    void main() {
        vec3 texColor = texture(texture_diffuse, TexCoords).rgb;

        float ambientStrength = 0.35;
        vec3 ambient = ambientStrength * vec3(1.0, 0.98, 0.95);

        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * vec3(1.0, 0.98, 0.95);

        float specularStrength = 0.5;
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        vec3 specular = specularStrength * spec * vec3(1.0, 1.0, 1.0);

        vec3 result = (ambient + diffuse + specular) * texColor;
        FragColor = vec4(result, 1.0);
    }
)";

const char* shadowVertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform vec3 lightPos;

    void main() {
        vec3 worldPos = vec3(model * vec4(aPos, 1.0));
        
        float floorY = -2.2;
        float t = (floorY - lightPos.y) / (worldPos.y - lightPos.y);
        vec3 shadowPos;
        shadowPos.x = lightPos.x + t * (worldPos.x - lightPos.x);
        shadowPos.y = floorY + 0.001;
        shadowPos.z = lightPos.z + t * (worldPos.z - lightPos.z);

        gl_Position = projection * view * vec4(shadowPos, 1.0);
    }
)";

const char* shadowFragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    void main() {
        FragColor = vec4(0.05, 0.05, 0.05, 0.45);
    }
)";

GLuint createProceduralTexture(bool isStem) {
    const int texSize = 256;
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    std::vector<unsigned char> pixels(texSize * texSize * 4);

    for (int y = 0; y < texSize; ++y) {
        for (int x = 0; x < texSize; ++x) {
            const int idx = (y * texSize + x) * 4;
            float nx = (float)x / (float)texSize;
            float ny = (float)y / (float)texSize;

            if (isStem) {
                float stemMask = 0.75f + 0.25f * std::sin((nx * 26.0f) + (ny * 18.0f));
                unsigned char g = static_cast<unsigned char>(clampTextureValue(75.0f + 90.0f * stemMask, 0.0f, 255.0f));
                pixels[idx + 0] = 50;
                pixels[idx + 1] = g;
                pixels[idx + 2] = 20;
                pixels[idx + 3] = 255;
            } else {
                float dist = std::sqrt((nx - 0.5f) * (nx - 0.5f) + (ny - 0.5f) * (ny - 0.5f));
                float blush = std::exp(-((nx - 0.25f) * (nx - 0.25f) + (ny - 0.35f) * (ny - 0.35f)) * 70.0f);
                float highlight = std::exp(-((nx - 0.75f) * (nx - 0.75f) + (ny - 0.18f) * (ny - 0.18f)) * 90.0f);
                float baseR = 180.0f + 55.0f * std::sin(nx * 12.0f + ny * 6.0f);
                float baseG = 25.0f + 20.0f * std::sin((nx + ny) * 10.0f);
                float baseB = 28.0f;

                float r = clampTextureValue(baseR - 70.0f * dist + 60.0f * blush + 40.0f * highlight, 0.0f, 255.0f);
                float g = clampTextureValue(baseG + 30.0f * (0.7f - dist) + 30.0f * blush, 0.0f, 255.0f);
                float b = clampTextureValue(baseB + 10.0f * (0.8f - dist), 0.0f, 255.0f);

                pixels[idx + 0] = static_cast<unsigned char>(r);
                pixels[idx + 1] = static_cast<unsigned char>(g);
                pixels[idx + 2] = static_cast<unsigned char>(b);
                pixels[idx + 3] = 255;
            }
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return textureID;
}

GLuint loadTexture(const char* path) {
    GLuint textureID = 0;
    glGenTextures(1, &textureID);

    int width = 1;
    int height = 1;
    int nrComponents = 4;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);

    if (!data) {
        std::cout << "Texture not found at: " << path << ". Generating procedural texture instead." << std::endl;

        const bool isStem = (std::strstr(path, "caule") != nullptr || std::strstr(path, "stem") != nullptr);
        return createProceduralTexture(isStem);
    }

    GLenum format = (nrComponents == 4) ? GL_RGBA : GL_RGB;
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

void generateAppleMesh(std::vector<float>& vertices, std::vector<unsigned int>& indices, float baseRadius, int rings, int sectors) {
    const float PI = 3.14159265359f;
    for (int r = 0; r <= rings; ++r) {
        float v = (float)r / rings;
        float phi = v * PI;

        for (int s = 0; s <= sectors; ++s) {
            float u = (float)s / sectors;
            float theta = u * 2.0f * PI;

            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
            float cosPhi = cos(phi);
            float sinPhi = sin(phi);

            float yNorm = cosPhi; 
            float rModifier = 1.0f + 0.22f * yNorm - 0.2f * yNorm * yNorm;
            float topIndent = 0.38f * exp(-9.0f * (1.0f - yNorm));
            float bottomIndent = 0.22f * exp(-9.0f * (1.0f + yNorm));

            float lobe = 1.0f + 0.03f * cos(5.0f * theta) * (1.0f - yNorm);

            float radius = (baseRadius * rModifier - topIndent - bottomIndent) * lobe;

            float x = cosTheta * sinPhi * radius;
            float y = yNorm * baseRadius - (topIndent - bottomIndent) * 0.5f;
            float z = sinTheta * sinPhi * radius;

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            glm::vec3 norm = glm::normalize(glm::vec3(x, y + (topIndent - bottomIndent), z));
            vertices.push_back(norm.x);
            vertices.push_back(norm.y);
            vertices.push_back(norm.z);

            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            unsigned int cur = r * (sectors + 1) + s;
            unsigned int next = (r + 1) * (sectors + 1) + s;

            indices.push_back(cur);
            indices.push_back(next);
            indices.push_back(cur + 1);

            indices.push_back(next);
            indices.push_back(next + 1);
            indices.push_back(cur + 1);
        }
    }
}

void generateStemMesh(std::vector<float>& vertices, std::vector<unsigned int>& indices, float length, float baseRadius, int segments) {
    const float PI = 3.14159265359f;
    for (int i = 0; i <= segments; ++i) {
        float t = (float)i / segments;
        float y = t * length;
        float curveX = 0.12f * t * t;
        float currentRadius = baseRadius * (1.0f - 0.35f * t);

        for (int j = 0; j <= 12; ++j) {
            float angle = (float)j / 12 * 2.0f * PI;
            float x = curveX + currentRadius * cos(angle);
            float z = currentRadius * sin(angle);

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            glm::vec3 norm = glm::normalize(glm::vec3(cos(angle), 0.15f, sin(angle)));
            vertices.push_back(norm.x);
            vertices.push_back(norm.y);
            vertices.push_back(norm.z);

            vertices.push_back((float)j / 12.0f);
            vertices.push_back(t);
        }
    }

    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < 12; ++j) {
            unsigned int cur = i * 13 + j;
            unsigned int next = (i + 1) * 13 + j;

            indices.push_back(cur);
            indices.push_back(next);
            indices.push_back(cur + 1);

            indices.push_back(next);
            indices.push_back(next + 1);
            indices.push_back(cur + 1);
        }
    }
}

GLuint createShaderProgram(const char* vShader, const char* fShader) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vShader, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fShader, NULL);
    glCompileShader(fs);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

void addApple() {
    if (g_Apples.size() >= 10) return;
    Apple a;
    int idx = g_Apples.size();
    a.position = glm::vec3((idx % 5 - 2) * 2.2f, (idx / 5) * 2.0f - 0.5f, 0.0f);
    a.size = 1.0f;
    a.rpm = 25.0f;
    a.currentAngle = 0.0f;
    a.rotationAxis = glm::vec3(0.0f, 1.0f, 0.15f);
    g_Apples.push_back(a);
}

void removeApple() {
    if (!g_Apples.empty()) g_Apples.pop_back();
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "3D Spinning Apples - Enhanced Realism", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    GLuint shadowShaderProgram = createShaderProgram(shadowVertexShaderSource, shadowFragmentShaderSource);

    GLuint appleTexture = loadTexture("Apple/Images/Ma .jpg");
    GLuint stemTexture  = loadTexture("Apple/Images/caule.jpg");

    std::vector<float> appleVertices;
    std::vector<unsigned int> appleIndices;
    generateAppleMesh(appleVertices, appleIndices, 0.9f, 48, 48);

    GLuint appleVAO, appleVBO, appleEBO;
    glGenVertexArrays(1, &appleVAO);
    glGenBuffers(1, &appleVBO);
    glGenBuffers(1, &appleEBO);

    glBindVertexArray(appleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, appleVBO);
    glBufferData(GL_ARRAY_BUFFER, appleVertices.size() * sizeof(float), appleVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, appleEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, appleIndices.size() * sizeof(unsigned int), appleIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    std::vector<float> stemVertices;
    std::vector<unsigned int> stemIndices;
    generateStemMesh(stemVertices, stemIndices, 0.38f, 0.04f, 12);

    GLuint stemVAO, stemVBO, stemEBO;
    glGenVertexArrays(1, &stemVAO);
    glGenBuffers(1, &stemVBO);
    glGenBuffers(1, &stemEBO);

    glBindVertexArray(stemVAO);
    glBindBuffer(GL_ARRAY_BUFFER, stemVBO);
    glBufferData(GL_ARRAY_BUFFER, stemVertices.size() * sizeof(float), stemVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, stemEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, stemIndices.size() * sizeof(unsigned int), stemIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    addApple();
    addApple();

    float lastFrameTime = 0.0f;
    glm::vec3 cameraPos(0.0f, 0.8f, 8.5f);
    glm::vec3 lightPos(4.0f, 6.0f, 4.0f);

    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Apple Controls");
        ImGui::Text("Number of Apples:");
        if (ImGui::Button("Add (+)") && g_Apples.size() < 10) addApple();
        ImGui::SameLine();
        if (ImGui::Button("Remove (-)") && g_Apples.size() > 1) removeApple();
        ImGui::Text("Count: %zu / 10", g_Apples.size());
        ImGui::Separator();

        for (size_t i = 0; i < g_Apples.size(); ++i) {
            std::string label = "Apple " + std::to_string(i + 1) + " RPM";
            ImGui::SliderFloat(label.c_str(), &g_Apples[i].rpm, 0.0f, 300.0f, "%.1f RPM");
        }
        ImGui::End();

        glClearColor(0.15f, 0.16f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, -0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);

        for (auto& apple : g_Apples) {
            apple.currentAngle += (apple.rpm * 6.0f) * deltaTime;
            if (apple.currentAngle > 360.0f) apple.currentAngle -= 360.0f;
        }

        glUseProgram(shadowShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shadowShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shadowShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shadowShaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));

        glBindVertexArray(appleVAO);
        for (const auto& apple : g_Apples) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, apple.position);
            model = glm::rotate(model, glm::radians(apple.currentAngle), apple.rotationAxis);
            glUniformMatrix4fv(glGetUniformLocation(shadowShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glDrawElements(GL_TRIANGLES, appleIndices.size(), GL_UNSIGNED_INT, 0);
        }

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));

        for (const auto& apple : g_Apples) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, apple.position);
            model = glm::rotate(model, glm::radians(apple.currentAngle), apple.rotationAxis);
            model = glm::scale(model, glm::vec3(apple.size));

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, appleTexture);
            glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse"), 0);
            glUniform1f(glGetUniformLocation(shaderProgram, "shininess"), 32.0f);
            glBindVertexArray(appleVAO);
            glDrawElements(GL_TRIANGLES, appleIndices.size(), GL_UNSIGNED_INT, 0);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, stemTexture);
            glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse"), 0);
            glUniform1f(glGetUniformLocation(shaderProgram, "shininess"), 4.0f);
            glBindVertexArray(stemVAO);
            glDrawElements(GL_TRIANGLES, stemIndices.size(), GL_UNSIGNED_INT, 0);
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &appleVAO);
    glDeleteBuffers(1, &appleVBO);
    glDeleteBuffers(1, &appleEBO);
    glDeleteVertexArrays(1, &stemVAO);
    glDeleteBuffers(1, &stemVBO);
    glDeleteBuffers(1, &stemEBO);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(shadowShaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
// Generates vertices and indices for a UV sphere
void generateSphere(float radius, unsigned int sectorCount, unsigned int stackCount,
                    std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    float x, y, z, xy;                              // vertex position
    float sectorStep = 2 * M_PI / sectorCount;
    float stackStep = M_PI / stackCount;
    float sectorAngle, stackAngle;

    // vertices
    for(unsigned int i = 0; i <= stackCount; ++i) {
        stackAngle = M_PI / 2 - i * stackStep;        // from pi/2 to -pi/2
        xy = radius * cosf(stackAngle);             // r * cos(u)
        z = radius * sinf(stackAngle);              // r * sin(u)

        // add (sectorCount+1) vertices per stack
        for(unsigned int j = 0; j <= sectorCount; ++j) {
            sectorAngle = j * sectorStep;           // from 0 to 2pi

            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)

            // vertex position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }

    // indices
    unsigned int k1, k2;
    for(unsigned int i = 0; i < stackCount; ++i) {
        k1 = i * (sectorCount + 1);     // beginning of current stack
        k2 = k1 + sectorCount + 1;      // beginning of next stack

        for(unsigned int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            // 2 triangles per sector excluding first and last stacks
            if(i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if(i != (stackCount-1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}

int main() {
    if(!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    auto* window =
      glfwCreateWindow(800, 600, "ChemistKit OpenGL", nullptr, nullptr);
    if(!window) return -1;

    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    while(!glfwWindowShouldClose(window)) {
        glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(window);
        glfwPollEvents();
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        generateSphere(1.0f, 36, 18, vertices, indices);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

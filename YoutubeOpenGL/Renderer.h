
#ifndef RENDERER_H
#define RENDERER_H

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Camera.h"

class Renderer {
private:
    Camera camera;

public:
    Renderer();

    void init(int width, int height);
    void display();
    void processInput(GLFWwindow* window);

    static void scrollCallback(
        GLFWwindow* window,
        double xoffset,
        double yoffset
    );
};

#endif


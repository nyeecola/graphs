#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

#include "constants.h"

// kills game with an error message
void force_quit(const char *str) {
    fprintf(stderr, "Fatal error: %s\n", str);
    fprintf(stderr, "GL Error Code: %d\n", glGetError());
    glfwTerminate();
    exit(-1);
}

int main(int argc, char **argv) {
    if (!glfwInit()) {
        force_quit("Could not initialize GLFW");
    }

    GLFWwindow *window = glfwCreateWindow(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, "Graphs", NULL, NULL);
    if (!window) {
        force_quit("Could not initialize GLFW");
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (gl3wInit()) {
        force_quit("Failed to initialize gl3w\n");
    }

    printf("%s\n", glGetString(GL_VERSION));

    if (!gl3wIsSupported(3, 0)) {
        force_quit("OpenGL 3.2 not supported\n");
    }

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.2, 0.6, 0.95, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}

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

// reads a text file and puts it inside a variable (this function allocates the buffer)
char *load_text_file_content(char *filename) {
    FILE *f = fopen(filename, "r");
    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    rewind(f);
    char *buffer = malloc((size + 1) * sizeof(*buffer));
    char *aux = buffer;
    char c;
    while ((c = fgetc(f)) != EOF) {
        *aux = c;
        aux++;
    }
    *aux = 0;
    fclose(f);
    return buffer;
}

int main(int argc, char **argv) {
    // glfw, gl3w and context initialization

    if (!glfwInit()) {
        force_quit("Could not initialize GLFW");
    }

    glfwWindowHint(GLFW_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_VERSION_MINOR, 3);
    GLFWwindow *window = glfwCreateWindow(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, "Graphs", NULL, NULL);
    if (!window) {
        force_quit("Could not initialize GLFW");
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (gl3wInit()) {
        force_quit("Failed to initialize gl3w\n");
    }

    printf("OpenGL version: %s\n", glGetString(GL_VERSION));

    if (!gl3wIsSupported(3, 3)) {
        force_quit("OpenGL 3.2 not supported\n");
    }

    // other initializations

    char *vertex_file_name = "src/vertexshader.glsl";
    char *frag_file_name = "src/fragshader.glsl";

    char *vertex_shader_content = load_text_file_content(vertex_file_name);
    char *frag_shader_content = load_text_file_content(frag_file_name);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.2, 0.6, 0.95, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}

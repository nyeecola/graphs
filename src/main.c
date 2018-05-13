#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>

#include "constants.h"

#define TRUE 1
#define FALSE 0
#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)

GLfloat global_zoom = 0.1f;
double global_delta_time;
bool global_clicked = FALSE;
double global_last_mouse_x = -1;
double global_last_mouse_y = -1;
double global_last_translation_x = 0;
double global_last_translation_y = 0;
double global_cur_translation_x = 0;
double global_cur_translation_y = 0;

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

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    // no-op
}

void cursor_pos_callback(GLFWwindow *window, double x, double y) {
    // no-op
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS) {
        global_clicked = true;
        glfwGetCursorPos(window, &global_last_mouse_x, &global_last_mouse_y);
    }
    if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_RELEASE && global_clicked) {
        global_clicked = false;
        global_last_translation_x += global_cur_translation_x;
        global_last_translation_y += global_cur_translation_y;
        global_cur_translation_x = 0;
        global_cur_translation_y = 0;
    }
}

// TODO: zoom on mouse cursor
void scroll_callback(GLFWwindow *window, double x, double y) {
    //global_zoom += 0.1 * y * global_delta_time;
    global_zoom += 0.02 * y;
    global_zoom = max(global_zoom, 0.04);
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
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    if (gl3wInit()) {
        force_quit("Failed to initialize gl3w\n");
    }

    printf("OpenGL version: %s\n", glGetString(GL_VERSION));

    if (!gl3wIsSupported(3, 3)) {
        force_quit("OpenGL 3.2 not supported\n");
    }

    // set draw target to back buffer
    glDrawBuffer(GL_BACK);

    // vsync
    glfwSwapInterval(1);


    // shader initialization

    char *vertex_file_name = "src/vertexshader.glsl";
    char *frag_file_name = "src/fragshader.glsl";

    const char *vertex_shader_content = load_text_file_content(vertex_file_name);
    const char *frag_shader_content = load_text_file_content(frag_file_name);

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertex_shader, 1, &vertex_shader_content, NULL);
    glShaderSource(frag_shader, 1, &frag_shader_content, NULL);

    GLint shader_compiled;
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &shader_compiled);
    if (shader_compiled != GL_TRUE) {
        GLchar message[1000];
        glGetShaderInfoLog(vertex_shader, 999, NULL, message);
        char output[1200];
        sprintf(output, "Vertex shader failed to compile\n%s\n", message);
        force_quit(output);
    }
    glCompileShader(frag_shader);
    if (shader_compiled != GL_TRUE) {
        GLchar message[1000];
        glGetShaderInfoLog(frag_shader, 999, NULL, message);
        char output[1200];
        sprintf(output, "Fragment shader failed to compile\n%s\n", message);
        force_quit(output);
    }

    GLuint shader_program = glCreateProgram();

    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, frag_shader);

    glLinkProgram(shader_program);
    GLint program_linked;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &program_linked);
    if (program_linked != GL_TRUE) {
        GLchar message[1000];
        glGetProgramInfoLog(program_linked, 999, NULL, message);
        char output[1200];
        sprintf(output, "Program failed to link\n%s\n", message);
        force_quit(output);
    }


    // buffers initialization

    GLuint VBO, EBO, VAO;
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glGenVertexArrays(1, &VAO);

    
    // initialize program data

    GLfloat circle_vertices[NUM_SECTIONS_CIRCLE * 3] = {0};
    for (int i = 0; i < NUM_SECTIONS_CIRCLE; i++) {
        float angle = i * (2 * M_PI / NUM_SECTIONS_CIRCLE);
        circle_vertices[i * 3] = cos(angle);
        circle_vertices[i * 3 + 1] = sin(angle);
        circle_vertices[i * 3 + 2] = 0.0f;
    }

    // TODO: check for error
    // TODO: really understand all of this better (this VAO thing)
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(circle_vertices), circle_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void *) 0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    GLint scale_uniform = glGetUniformLocation(shader_program, "scale");
    GLint aspect_ratio_uniform = glGetUniformLocation(shader_program, "aspect_ratio");
    GLint translation_uniform = glGetUniformLocation(shader_program, "translation");

    double last_time = glfwGetTime();
    global_delta_time = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwWaitEvents();
        double current_time = glfwGetTime();
        global_delta_time = current_time - last_time;
        last_time = current_time;

        assert(global_zoom > 0);

        double current_mouse_x, current_mouse_y;
        glfwGetCursorPos(window, &current_mouse_x, &current_mouse_y);
        if (global_clicked) {
            global_cur_translation_x = (current_mouse_x - global_last_mouse_x);
            global_cur_translation_x = (global_cur_translation_x / (DEFAULT_SCREEN_WIDTH/2));
            global_cur_translation_x /= global_zoom;

            global_cur_translation_y = (current_mouse_y - global_last_mouse_y);
            global_cur_translation_y = (global_cur_translation_y / (DEFAULT_SCREEN_HEIGHT/2)) / ((float) DEFAULT_SCREEN_WIDTH / (float) DEFAULT_SCREEN_HEIGHT);
            global_cur_translation_y *= -1;
            global_cur_translation_y /= global_zoom;
        }

        glClearColor(0.2, 0.6, 0.95, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader_program);
        glUniform1f(scale_uniform, global_zoom);
        glUniform1f(aspect_ratio_uniform, (float) DEFAULT_SCREEN_WIDTH / (float) DEFAULT_SCREEN_HEIGHT);

        glBindVertexArray(VAO);
        double frame_translation_x = global_last_translation_x + global_cur_translation_x;
        double frame_translation_y = global_last_translation_y + global_cur_translation_y;
        glUniform3f(translation_uniform, frame_translation_x + 0.0f, frame_translation_y + 0.0f, 0.0f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, NUM_SECTIONS_CIRCLE);
        glUniform3f(translation_uniform, frame_translation_x + 2.0f, frame_translation_y + 0.22f, 0.0f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, NUM_SECTIONS_CIRCLE);
        glUniform3f(translation_uniform, frame_translation_x + -1.4f, frame_translation_y + 2.22f, 0.0f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, NUM_SECTIONS_CIRCLE);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        //glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}

#include <GL/gl3w.h>
#include <GL/gl3w.c>
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

typedef struct {
    double x;
    double y;
} v2f;

v2f V2F(float x, float y) {
    v2f v = {x, y};
    return v;
}

v2f add(v2f v1, v2f v2) {
    v2f v = {v1.x + v2.x, v1.y + v2.y};
    return v;
}

v2f sub(v2f v1, v2f v2) {
    v2f v = {v1.x - v2.x, v1.y - v2.y};
    return v;
}

float dot(v2f v1, v2f v2) {
    return v1.x * v2.x + v1.y * v2.y;
}

float magnitude(v2f v1) {
    return sqrt(v1.x * v1.x + v1.y * v1.y);
}

typedef struct {
    v2f pos;
    bool selected;
    int *children;
    int num_children;
} vertex_t;

typedef struct {
    GLfloat zoom;
    double delta_time;
    bool dragging_map;
    bool dragging_vertex;
    int modifying_vertex; // NOTE: -1 means no vertex is currently being modified
    v2f last_mouse;
    v2f last_translation;
    v2f cur_translation;
    vertex_t *circles;
    int num_circles;
} global_state_t;

void create_vertex(global_state_t *global_state, double x, double y) {
    vertex_t v;
    v.pos.x = x;
    v.pos.y = y;
    v.selected = FALSE;
    v.children = malloc(MAX_VERTICES * sizeof(*v.children));
    v.num_children = 0;
    global_state->circles[global_state->num_circles++] = v;
}

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
    global_state_t *global_state = glfwGetWindowUserPointer(window);
    if (global_state == NULL) { // program not fully initilized yet
        return;
    }

    // no-op
}

void cursor_pos_callback(GLFWwindow *window, double x, double y) {
    global_state_t *global_state = glfwGetWindowUserPointer(window);
    if (global_state == NULL) { // program not fully initilized yet
        return;
    }

    // no-op
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    global_state_t *global_state = glfwGetWindowUserPointer(window);
    if (global_state == NULL) { // program not fully initilized yet
        return;
    }

    if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS && mods | GLFW_MOD_CONTROL) {
        v2f temp;
        glfwGetCursorPos(window, &temp.x, &temp.y);
        temp.x = (temp.x / (DEFAULT_SCREEN_WIDTH / 2) - 1.0f) / global_state->zoom;
        temp.x -= global_state->last_translation.x;
        temp.y = (temp.y / (DEFAULT_SCREEN_HEIGHT / 2) - 1.0f) / global_state->zoom;
        temp.y /= ((float) DEFAULT_SCREEN_WIDTH / (float) DEFAULT_SCREEN_HEIGHT);
        temp.y += global_state->last_translation.y;

        global_state->modifying_vertex = -1; // unselect vertices
        for (int i = 0; i < global_state->num_circles; i++) {
            double x = global_state->circles[i].pos.x - temp.x;
            double y = -global_state->circles[i].pos.y - temp.y;
            double r = 1.0f;
            if (x * x + y * y <= r * r) {
                global_state->modifying_vertex = i;
                break;
            }
        }
    }

    // handle vertice selection and dragging
    if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS && !mods) {
        global_state->dragging_vertex = TRUE;

        v2f temp;
        glfwGetCursorPos(window, &temp.x, &temp.y);
        temp.x = (temp.x / (DEFAULT_SCREEN_WIDTH / 2) - 1.0f) / global_state->zoom;
        temp.x -= global_state->last_translation.x;
        temp.y = (temp.y / (DEFAULT_SCREEN_HEIGHT / 2) - 1.0f) / global_state->zoom;
        temp.y /= ((float) DEFAULT_SCREEN_WIDTH / (float) DEFAULT_SCREEN_HEIGHT);
        temp.y += global_state->last_translation.y;
        for (int i = 0; i < global_state->num_circles; i++) {
            double x = global_state->circles[i].pos.x - temp.x;
            double y = -global_state->circles[i].pos.y - temp.y;
            double r = 1.0f;
            if (x * x + y * y <= r * r) {
                global_state->circles[i].selected = true;
            } else {
                global_state->circles[i].selected = false;
            }
        }
    }
    if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_RELEASE) {
        global_state->dragging_vertex = FALSE;
        if (global_state->modifying_vertex != -1) {
            v2f temp;
            glfwGetCursorPos(window, &temp.x, &temp.y);
            temp.x = (temp.x / (DEFAULT_SCREEN_WIDTH / 2) - 1.0f) / global_state->zoom;
            temp.x -= global_state->last_translation.x;
            temp.y = (temp.y / (DEFAULT_SCREEN_HEIGHT / 2) - 1.0f) / global_state->zoom;
            temp.y /= ((float) DEFAULT_SCREEN_WIDTH / (float) DEFAULT_SCREEN_HEIGHT);
            temp.y += global_state->last_translation.y;

            for (int i = 0; i < global_state->num_circles; i++) {
                double x = global_state->circles[i].pos.x - temp.x;
                double y = -global_state->circles[i].pos.y - temp.y;
                double r = 1.0f;
                if (x * x + y * y <= r * r) {
                    // TODO: check if vertice doesn't already belong to children
                    int vertex = global_state->modifying_vertex;
                    global_state->circles[vertex].children[global_state->circles[vertex].num_children++] = i;
                    break;
                }
            }
        }
        global_state->modifying_vertex = -1;
    }

    // handle map dragging
    if (button == GLFW_MOUSE_BUTTON_2 && action == GLFW_PRESS) {
        global_state->dragging_map = true;
        glfwGetCursorPos(window, &global_state->last_mouse.x, &global_state->last_mouse.y);
    }
    if (button == GLFW_MOUSE_BUTTON_2 && action == GLFW_RELEASE && global_state->dragging_map) {
        global_state->dragging_map = false;
        global_state->last_translation.x += global_state->cur_translation.x;
        global_state->last_translation.y += global_state->cur_translation.y;
        global_state->cur_translation.x = 0;
        global_state->cur_translation.y = 0;
    }
}

// TODO: zoom on mouse cursor
void scroll_callback(GLFWwindow *window, double x, double y) {
    global_state_t *global_state = glfwGetWindowUserPointer(window);
    if (global_state == NULL) { // program not fully initilized yet
        return;
    }

    //global_state->zoom += 0.1 * y * global_state->delta_time;
    global_state->zoom += 0.02 * y;
    global_state->zoom = max(global_state->zoom, 0.04);
}

int main(int argc, char **argv) {
    // glfw, gl3w and context initialization

    if (!glfwInit()) {
        force_quit("Could not initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    //glfwWindowHint(GLFW_STENCIL_BITS, 4); // TODO: understand what is this
    glfwWindowHint(GLFW_SAMPLES, 4);
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
        force_quit("OpenGL 3.3 not supported\n");
    }

    // set draw target to back buffer
    glDrawBuffer(GL_BACK);

    // vsync
    glfwSwapInterval(1);

    // enable muiltisampling and blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendFunc(GL_SRC_ALPHA_SATURATE, GL_ONE);
    //glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    //glEnable(GL_POLYGON_SMOOTH);
    glEnable(GL_MULTISAMPLE);

    // z-buffer
    glEnable(GL_DEPTH_TEST);
    //glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    //glDepthFunc(GL_LESS);

    // line width
    glLineWidth(3);

#if 1
    // debug
    int samples;
    glGetIntegerv(GL_SAMPLES, &samples);
    printf("Samples: %d\n", samples);
    glGetIntegerv(GL_MAJOR_VERSION, &samples);
    printf("Major: %d\n", samples);
    glGetIntegerv(GL_MINOR_VERSION, &samples);
    printf("Minor: %d\n", samples);
#endif


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

    // vertices buffers
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);

    // line buffers
    GLuint VBO2, VAO2;
    glGenBuffers(1, &VBO2);
    glGenVertexArrays(1, &VAO2);
    
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
    GLint color_uniform = glGetUniformLocation(shader_program, "color");


    // initialize global state
    global_state_t global_state;
    global_state.zoom = 0.1f;
    global_state.delta_time = 0;
    global_state.dragging_map = FALSE;
    global_state.dragging_vertex = FALSE;
    global_state.modifying_vertex = -1; // no vertex currently selected
    global_state.last_mouse.x = -1;
    global_state.last_mouse.y = -1;
    global_state.last_translation.x = 0;
    global_state.last_translation.y = 0;
    global_state.cur_translation.x = 0;
    global_state.cur_translation.y = 0;
    global_state.circles = malloc(MAX_VERTICES * sizeof(*global_state.circles));
    global_state.num_circles = 0;
    // TODO: actually implement a way to add/remove circles
    // TEMP: add some circles just for testing purposes
    create_vertex(&global_state, -0.5, -0.4);
    create_vertex(&global_state, 2.2, 0.7);
    create_vertex(&global_state, -1.4, 2.1);

    glfwSetWindowUserPointer(window, (void *) &global_state);

    double last_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        //glfwPollEvents();
        glfwWaitEvents();

        double current_time = glfwGetTime();
        global_state.delta_time = current_time - last_time;
        last_time = current_time;

        assert(global_state.zoom > 0);

        v2f current_mouse;
        glfwGetCursorPos(window, &current_mouse.x, &current_mouse.y);
        if (global_state.dragging_map) {
            global_state.cur_translation.x = (current_mouse.x - global_state.last_mouse.x);
            global_state.cur_translation.x = (global_state.cur_translation.x / (DEFAULT_SCREEN_WIDTH/2));
            global_state.cur_translation.x /= global_state.zoom;

            global_state.cur_translation.y = (current_mouse.y - global_state.last_mouse.y);
            global_state.cur_translation.y = (global_state.cur_translation.y / (DEFAULT_SCREEN_HEIGHT/2));
            global_state.cur_translation.y /= ((float) DEFAULT_SCREEN_WIDTH / (float) DEFAULT_SCREEN_HEIGHT);
            global_state.cur_translation.y *= -1;
            global_state.cur_translation.y /= global_state.zoom;
        }
        // TODO; make sure this works if currently also dragging map
        // TODO: make it possible to drag multiple vertices simultaneously
        if (global_state.dragging_vertex) {
            v2f temp = {current_mouse.x, current_mouse.y};
            temp.x = (temp.x / (DEFAULT_SCREEN_WIDTH / 2) - 1.0f) / global_state.zoom;
            temp.x -= global_state.last_translation.x;
            temp.y = (temp.y / (DEFAULT_SCREEN_HEIGHT / 2) - 1.0f) / global_state.zoom;
            temp.y /= ((float) DEFAULT_SCREEN_WIDTH / (float) DEFAULT_SCREEN_HEIGHT);
            temp.y += global_state.last_translation.y;
            for (int i = 0; i < global_state.num_circles; i++) {
                if (global_state.circles[i].selected) {
                    global_state.circles[i].pos.x = temp.x;
                    global_state.circles[i].pos.y = -temp.y;
                }
            }
        }

        glClearColor(0.95, 0.6, 0.3, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader_program);
        glUniform1f(scale_uniform, global_state.zoom);
        glUniform1f(aspect_ratio_uniform, (float) DEFAULT_SCREEN_WIDTH / (float) DEFAULT_SCREEN_HEIGHT);

        // screen position
        v2f frame_translation;
        frame_translation.x = global_state.last_translation.x + global_state.cur_translation.x;
        frame_translation.y = global_state.last_translation.y + global_state.cur_translation.y;

        // draw vertices
        glBindVertexArray(VAO);
        for (int i = 0; i < global_state.num_circles; i++) {
            double x = frame_translation.x + global_state.circles[i].pos.x;
            double y = frame_translation.y + global_state.circles[i].pos.y;
            glUniform3f(translation_uniform, x, y, 0.0f);
            if (global_state.circles[i].selected) {
                glUniform3f(color_uniform, 0.8f, 0.8f, 0.8f);
            } else {
                glUniform3f(color_uniform, 1.0f, 1.0f, 1.0f);
            }
            glDrawArrays(GL_TRIANGLE_FAN, 0, NUM_SECTIONS_CIRCLE);
        }
        glBindVertexArray(0);

        // draw arrows
        // TODO: add direction
        glBindVertexArray(VAO2);
        glUniform3f(translation_uniform, 0, 0, 0.0f);
        glUniform3f(color_uniform, 0.0f, 0.0f, 0.0f);

        // vertices children
        for (int i = 0; i < global_state.num_circles; i++) {
            for (int j = 0; j < global_state.circles[i].num_children; j++) {
                double x1 = frame_translation.x + global_state.circles[i].pos.x;
                double y1 = frame_translation.y + global_state.circles[i].pos.y;
                double x2 = frame_translation.x + global_state.circles[global_state.circles[i].children[j]].pos.x; 
                double y2 = frame_translation.y + global_state.circles[global_state.circles[i].children[j]].pos.y; 
                GLfloat line_vertices[3 * 2] = {
                    x1, y1, 0.2,
                    x2, y2, 0.2,
                };
                glBindBuffer(GL_ARRAY_BUFFER, VBO2);
                glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void *) 0);
                glEnableVertexAttribArray(0);
                glDrawArrays(GL_LINES, 0, 2);
            }
        }

        // arrow being currently created
        if (global_state.modifying_vertex != -1) {
            double x1 = frame_translation.x + global_state.circles[global_state.modifying_vertex].pos.x;
            double y1 = frame_translation.y + global_state.circles[global_state.modifying_vertex].pos.y;
            double x2 = ((frame_translation.x + current_mouse.x) / (DEFAULT_SCREEN_WIDTH / 2) - 1.0f) / global_state.zoom;
            double y2 = ((frame_translation.y + current_mouse.y) / (DEFAULT_SCREEN_HEIGHT / 2) - 1.0f) / global_state.zoom;
            y2 /= ((float) DEFAULT_SCREEN_WIDTH / (float) DEFAULT_SCREEN_HEIGHT);
            GLfloat line_vertices[3 * 2] = {
                x1, y1, 0.2,
                x2, -y2, 0.2,
            };
            glBindBuffer(GL_ARRAY_BUFFER, VBO2);
            glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void *) 0);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_LINES, 0, 2);
        }

        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glfwTerminate();

    return 0;
}

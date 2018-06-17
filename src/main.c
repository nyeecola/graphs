#include <GL/gl3w.h>
#include <GL/gl3w.c>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
    #undef max
    #undef min
    #define M_PI 3.14159f
#endif

#include "font.h"
#include "constants.h"

#define TRUE 1
#define FALSE 0
#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)

#define NUMERIC_TYPE double
#define TYPE_NAME v2f
#include "math.c"
#undef NUMERIC_TYPE
#undef TYPE_NAME

typedef struct {
    //int orig; // NOTE: unused
    int dest;
    int weight;

    v2f weight_pos_screen;
} edge_t;

typedef struct {
    int weight;
    v2f pos;
    bool selected;
    edge_t *children;
    int num_children;

    int filled; // 0 means not found, 1 means filling, 2 means filled
    int16_t fill_entrance_index[MAX_VERTEX_ENTRANCES]; // index of the "father"
    float fill_radius[MAX_VERTEX_ENTRANCES];
    int num_fill_entrances;
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
    int editing_circle; // NOTE: -1 means no vertex is currently being edited (weight)
    edge_t *editing_edge; // NOTE: NULL means no edge is currently being edited
    // NOTE: a edge and a circle can't both be edited at the same time
    char temp_weight_str[10];
    bool showing_menu;

    int current_animation_root; // index of the current animation root vertex

    GLuint default_vao;
    GLuint edge_vao;
    GLuint edge_vbo;
    GLuint font_vao;
    GLuint font_vbo; // we should probably cache this
    GLuint menu_vao;
} global_state_t;

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
    assert(f);
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

void export(global_state_t *global_state, char *filename) {
    FILE *f = fopen(filename, "w");
    assert(f);
    int count_edges = 0;
    for (int i = 0; i < global_state->num_circles; i++) {
        count_edges += global_state->circles[i].num_children;
    }
    fprintf(f, "%d %d\n", global_state->num_circles, count_edges);
    for (int i = 0; i < global_state->num_circles; i++) {
        int x = (int) (global_state->circles[i].pos.x * 4);
        int y = (int) (global_state->circles[i].pos.y * 4);
        fprintf(f, "%d %d %d %d\n", i, x, y, global_state->circles[i].weight);
    }
    for (int i = 0; i < global_state->num_circles; i++) {
        for (int j = 0; j < global_state->circles[i].num_children; j++) {
            edge_t edge = global_state->circles[i].children[j];
            fprintf(f, "%d %d %d\n", i, edge.dest, edge.weight);
        }
    }
    fprintf(f, "%d\n", rand() % global_state->num_circles);
    fclose(f);
}

GLuint initialize_shader(char *vertex_file_name, char *frag_file_name) {
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

    return shader_program;
}

void font_render_text_horrible(GLuint font_shader_program, int vbo, int vao, stbtt_bakedchar *cdata, int ftex, float x, float y, char *text, float r, float g, float b, bool centered) {
    if (centered) {
        char *aux = text;
        float x_off = 0, y_off = 0;
        while (*aux) {
            stbtt_bakedchar baked_char = cdata[*aux - 32];
            x_off += baked_char.xadvance;
            y_off = min(y_off, baked_char.yoff);
            aux++;
        }
        x -= x_off / 2;
        y -= y_off / 2;
    }

    x -= DEFAULT_SCREEN_WIDTH / 2;
    y = DEFAULT_SCREEN_HEIGHT / 2 - y;

    // TODO: clean all of this up
    // assuming orthographic projection with units = screen pixels, origin at top left
    glEnable(GL_TEXTURE_2D);
    glUseProgram(font_shader_program);
    glUniform1i(glGetUniformLocation(font_shader_program, "tex"), 0);
    glUniform1f(glGetUniformLocation(font_shader_program, "window_width"), DEFAULT_SCREEN_WIDTH);
    glUniform1f(glGetUniformLocation(font_shader_program, "window_height"), DEFAULT_SCREEN_HEIGHT);
    glUniform3f(glGetUniformLocation(font_shader_program, "color"), r, g, b);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    while (*text) {
        if (*text >= 32 && *text < 128) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, 512, 512, *text-32, &x, &y, &q, 1);
            stbtt_bakedchar baked_char = cdata[*text - 32]; // TODO: remove this magic number
            //printf("%f %f %f %f %f\n", q.x0, q.y0, q.x1, q.y1, baked_char.yoff);
            if (*text == 'e' || *text == 'a' || *text == 'd' || *text == 'o' || *text == 'u' || *text == 's'
                    || *text == 'c' || *text == 't' || *text == '/' || *text == 'C' || *text == 'S'
                    || *text == 'O' || *text == 'U' || *text == '3' || *text == '5') {
                baked_char.yoff += 1.0f;
            }
            if (*text == 'g' || *text == 'p' || *text == 'q') {
                baked_char.yoff += 4.0f;
            }

            float buffer[6 * 5] = {
                q.x0, q.y0-baked_char.yoff, -0.4, q.s0, q.t1,
                q.x1, q.y0-baked_char.yoff, -0.4, q.s1, q.t1,
                q.x0, q.y1-baked_char.yoff, -0.4, q.s0, q.t0,
                q.x1, q.y0-baked_char.yoff, -0.4, q.s1, q.t1,
                q.x1, q.y1-baked_char.yoff, -0.4, q.s1, q.t0,
                q.x0, q.y1-baked_char.yoff, -0.4, q.s0, q.t0
            };

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ftex);

            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            // NOTE: OpenGL hack (Buffer Object Streaming) to improve performance
            glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), NULL, GL_STREAM_DRAW);
            glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindVertexArray(0);
        }
        text++;
    }
}

void clear_flood(global_state_t *global_state) {
    vertex_t *circles = global_state->circles;
    for (int i = 0; i < global_state->num_circles; i++) {
        circles[i].filled = 0;
        circles[i].num_fill_entrances = 0;
        memset(circles[i].fill_radius, 0, sizeof(circles[i].fill_radius));
    }
}

void BFS(global_state_t *global_state, int root_index) {
    clear_flood(global_state);
    vertex_t *circles = global_state->circles;

    circles[root_index].filled = 1;
    circles[root_index].fill_entrance_index[circles[root_index].num_fill_entrances++] = root_index;
    global_state->current_animation_root = root_index;
    int *visited = calloc(MAX_VERTICES, sizeof(*visited));

    int *queue = calloc(MAX_VERTICES, sizeof(*queue));
    int queue_start = 0;
    int queue_end = 0;
    queue[queue_end++] = root_index;
    visited[root_index] = 1;
    while (queue_start < queue_end) {
        int node = queue[queue_start++];

        // used for animation
        if (visited[node] == 2) {
            break;
        }
        visited[node] = 2;

        for (int i = 0; i < circles[node].num_children; i++) {
            int children_index = circles[node].children[i].dest;
#if 1
            // multi_entrance animation enabled

            if (!visited[children_index]) { // used for animation
                queue[queue_end++] = children_index;
                visited[children_index] = 1;
            }
            if (visited[children_index] != 2) {
                circles[children_index].filled = 1;
                circles[children_index].fill_entrance_index[circles[children_index].num_fill_entrances++] = node;
            }
#else
            // multi_entrance animation disabled

            if (!visited[children_index]) { // NOTE: disabled for animation
                queue[queue_end++] = children_index;
                visited[children_index] = 1;
                circles[children_index].filled = 1;
                circles[children_index].fill_entrance_index[circles[children_index].num_fill_entrances++] = node;
            }
#endif
        }
    }
    free(visited);
    free(queue);
}

v2f get_untranslated_world_space(GLFWwindow *window, double zoom, v2f v) {
    double x = (v.x / (DEFAULT_SCREEN_WIDTH / 2) - 1.0f) / zoom;
    double y = -((v.y / (DEFAULT_SCREEN_HEIGHT / 2) - 1.0f) / zoom) / ASPECT_RATIO;
    v2f r = {x, y};
    return r;
}

v2f get_cursor_untranslated_world_space(GLFWwindow *window, double zoom) {
    v2f current_mouse;
    glfwGetCursorPos(window, &current_mouse.x, &current_mouse.y);
    v2f r = get_untranslated_world_space(window, zoom, current_mouse);
    return r;
}

v2f get_cursor_world_space(GLFWwindow *window, v2f translation, double zoom) {
    v2f untranslated = get_cursor_untranslated_world_space(window, zoom);
    v2f r = sub_v2f(untranslated, translation);
    return r;
}

void create_vertex(global_state_t *global_state, v2f p, int weight) {
    vertex_t v;
    v.weight = weight;
    v.pos = p;
    v.selected = FALSE;
    v.children = malloc(MAX_VERTICES * sizeof(*v.children));
    v.num_children = 0;
    v.filled = 0;
    v.num_fill_entrances = 0;
    global_state->circles[global_state->num_circles++] = v;
}

void delete_vertex(global_state_t *global_state, int index) {
    clear_flood(global_state);

    global_state->dragging_vertex = FALSE;
    for (int i = 0; i < global_state->num_circles; i++) {
        int removed_location = -1;
        for (int j = 0; j < global_state->circles[i].num_children; j++) {
            if (global_state->circles[i].children[j].dest == index) {
                assert(removed_location == -1);
                removed_location = j;
            } else if (global_state->circles[i].children[j].dest > index) {
                global_state->circles[i].children[j].dest--;
            }
        }
        if (removed_location != -1) {
            for (int j = removed_location + 1; j < global_state->circles[i].num_children; j++) {
                global_state->circles[i].children[j-1] = global_state->circles[i].children[j];
            }
            global_state->circles[i].num_children--;
        }
    }

    free(global_state->circles[index].children);
    for (int i = index + 1; i < global_state->num_circles; i++) {
        global_state->circles[i-1] = global_state->circles[i];
    }
    global_state->num_circles--;
    global_state->editing_circle = -1;
    global_state->editing_edge = NULL;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    global_state_t *global_state = glfwGetWindowUserPointer(window);
    if (global_state == NULL) { // program not fully initilized yet
        return;
    }

    v2f cursor_pos = get_cursor_world_space(window, global_state->last_translation, global_state->zoom);

    // show help when TAB is pressed
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        global_state->showing_menu = !global_state->showing_menu;
    }

    // export to file when E is pressed
    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        export(global_state, "output.txt");
    }

    // randomize all weights when R is pressed
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        for (int i = 0; i < global_state->num_circles; i++) {
            global_state->circles[i].weight = rand() % (WEIGHT_RANDOM_LIMIT + 1);
            for (int j = 0; j < global_state->circles[i].num_children; j++) {
                global_state->circles[i].children[j].weight = rand() % (WEIGHT_RANDOM_LIMIT + 1);
            }
        }
    }

    // make graph complete when C is pressed
    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        for (int i = 0; i < global_state->num_circles; i++) {
            bool *missing = calloc(global_state->num_circles, sizeof(*missing));
            for (int j = 0; j < global_state->circles[i].num_children; j++) {
                missing[global_state->circles[i].children[j].dest] = 1;
            }
            for (int j = 0; j < global_state->num_circles; j++) {
                if (!missing[j] && i != j) {
                    int cur_num_children = global_state->circles[i].num_children;
                    global_state->circles[i].children[cur_num_children].dest = j;
                    global_state->circles[i].children[cur_num_children].weight = 1;
                    global_state->circles[i].num_children++;
                }
            }
            free(missing);
        }
    }

    // create vertex when A is pressed
    if (key == GLFW_KEY_A && action == GLFW_PRESS) {
        bool found = false;
        for (int i = 0; i < global_state->num_circles; i++) {
            v2f p = sub_v2f(global_state->circles[i].pos, cursor_pos);
            double r = 1.0f;
            if (p.x * p.x + p.y * p.y <= r * r) {
                found = true;
                break;
            }
        }
        if (!found) {
            create_vertex(global_state, cursor_pos, 1);
        }
    }

    // delete vertex when D is pressed
    if (key == GLFW_KEY_D && action == GLFW_PRESS) {
        for (int i = 0; i < global_state->num_circles; i++) {
            v2f p = sub_v2f(global_state->circles[i].pos, cursor_pos);
            double r = 1.0f;
            if (p.x * p.x + p.y * p.y <= r * r) {
                delete_vertex(global_state, i);
                break;
            }
        }
    }

    // handle changing weights when X is pressed (NOTE: user must press ENTER to finalize or ESC to cancel)
    {
        if (key == GLFW_KEY_X && action == GLFW_PRESS) {
            global_state->editing_circle = -1;
            global_state->editing_edge = NULL;

            // vertex weights
            bool found = false;
            for (int i = 0; i < global_state->num_circles && !found; i++) {
                v2f p = sub_v2f(global_state->circles[i].pos, cursor_pos);
                double r = 1.0f;
                if (p.x * p.x + p.y * p.y <= r * r) {
                    global_state->editing_circle = i;
                    global_state->temp_weight_str[0] = 0;
                    found = true;
                }
            }
            
            // edge weights
            for (int i = 0; i < global_state->num_circles && !found; i++) {
                for (int j = 0; j < global_state->circles[i].num_children && !found; j++) {
                    edge_t *edge = &global_state->circles[i].children[j];

                    v2f screen_space_cursor_pos;
                    glfwGetCursorPos(window, &screen_space_cursor_pos.x, &screen_space_cursor_pos.y);

                    v2f p = sub_v2f(edge->weight_pos_screen, screen_space_cursor_pos);
    
                    double r = FONT_SIZE;
                    if (p.x * p.x + p.y * p.y <= r * r) {
                        global_state->editing_edge = edge;
                        global_state->temp_weight_str[0] = 0;
                        found = true;
                    }
                }
            }
        }
        if ((global_state->editing_circle != -1 || global_state->editing_edge) && action == GLFW_PRESS) {
            if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
                if (strlen(global_state->temp_weight_str) != 10) {
                    assert(strlen(global_state->temp_weight_str) < 10);
                    char temp_str[2] = "";
                    temp_str[0] = '0' + key - GLFW_KEY_0;
                    strcat(global_state->temp_weight_str, temp_str);
                    if (global_state->editing_circle != -1) {
                        global_state->circles[global_state->editing_circle].weight = atoi(global_state->temp_weight_str);
                    } else {
                        global_state->editing_edge->weight = atoi(global_state->temp_weight_str);
                    }
                }
            }
            if (key == GLFW_KEY_BACKSPACE) {
                int len = strlen(global_state->temp_weight_str);
                if (len > 0) {
                    global_state->temp_weight_str[len-1] = 0;
                } else {
                    strcpy(global_state->temp_weight_str, "0");
                }
                if (global_state->editing_circle != -1) {
                    global_state->circles[global_state->editing_circle].weight = atoi(global_state->temp_weight_str);
                } else {
                    global_state->editing_edge->weight = atoi(global_state->temp_weight_str);
                }
            }
            if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER|| key == GLFW_KEY_ESCAPE) {
                global_state->editing_circle = -1;
                global_state->editing_edge = NULL;
            }
        }
    }

    // run BFS when B is pressed
    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        for (int i = 0; i < global_state->num_circles; i++) {
            v2f p = sub_v2f(global_state->circles[i].pos, cursor_pos);
            double r = 1.0f;
            if (p.x * p.x + p.y * p.y <= r * r) {
                BFS(global_state, i);
                break;
            }
        }
    }
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    global_state_t *global_state = glfwGetWindowUserPointer(window);
    if (global_state == NULL) { // program not fully initilized yet
        return;
    }

    v2f mouse_pos = get_cursor_world_space(window, global_state->last_translation, global_state->zoom);

    if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS) {
        // handle adding dependencies
        if (mods & GLFW_MOD_CONTROL) {
            global_state->modifying_vertex = -1; // unselect vertices
            for (int i = 0; i < global_state->num_circles; i++) {
                v2f p = sub_v2f(global_state->circles[i].pos, mouse_pos);
                double r = 1.0f;
                if (p.x * p.x + p.y * p.y <= r * r) {
                    global_state->modifying_vertex = i;
                    break;
                }
            }
        }
    }

    // handle vertice selection and dragging
    {
        if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS && !mods) {
            global_state->dragging_vertex = TRUE;
            for (int i = 0; i < global_state->num_circles; i++) {
                global_state->circles[i].selected = false;
            }
            for (int i = 0; i < global_state->num_circles; i++) {
                v2f p = sub_v2f(global_state->circles[i].pos, mouse_pos);
                double r = 1.0f;
                if (p.x * p.x + p.y * p.y <= r * r) {
                    global_state->circles[i].selected = true;
                    break;
                }
            }
        }

        if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_RELEASE) {
            global_state->dragging_vertex = FALSE;
            if (global_state->modifying_vertex != -1) {
                int vertex = global_state->modifying_vertex;
                for (int i = 0; i < global_state->num_circles; i++) {
                    v2f p = sub_v2f(global_state->circles[i].pos, mouse_pos);
                    double r = 1.0f;
                    if (p.x * p.x + p.y * p.y <= r * r) {
                        // check if vertice doesn't already belong to children
                        bool is_child_already = false;
                        for (int j = 0; j < global_state->circles[vertex].num_children; j++) {
                            if (global_state->circles[vertex].children[j].dest == i) {
                                is_child_already = true;
                            }
                        }

                        if (!is_child_already && vertex != i) {
                            int cur_num_children = global_state->circles[vertex].num_children;
                            global_state->circles[vertex].children[cur_num_children].dest = i;
                            global_state->circles[vertex].children[cur_num_children].weight = 1;
                            global_state->circles[vertex].num_children++;
                            break;
                        }
                    }
                }
            }
            global_state->modifying_vertex = -1;
        }
    }

    // handle map dragging
    if (button == GLFW_MOUSE_BUTTON_2 && action == GLFW_PRESS) {
        global_state->dragging_map = true;
        glfwGetCursorPos(window, &global_state->last_mouse.x, &global_state->last_mouse.y);
    }
    if (button == GLFW_MOUSE_BUTTON_2 && action == GLFW_RELEASE && global_state->dragging_map) {
        global_state->dragging_map = false;
        global_state->last_translation = add_v2f(global_state->last_translation, global_state->cur_translation);
        global_state->cur_translation = create_v2f(0, 0);
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

// TODO: optimize this, its performance is terrible right now
void draw_edge(edge_t *edge, int VBO, int VAO, int VBO3, int VAO3, global_state_t *global_state, v2f v1,
               v2f v2, bool curved, int weight_color_r, int weight_color_g, int weight_color_b,
               int program_arrow, int program_font, stbtt_bakedchar *cdata, int ftex) {

    glUseProgram(program_arrow);
    // draw arrow body

    v2f middle_point_on_curve;
    if (curved) {
        // calculates the middle point used for bezier
        v2f half_v1v2_origin = scale_v2f(sub_v2f(v2, v1), 0.5f);
        v2f middle_point = add_v2f(v1, half_v1v2_origin);
        v2f ortho_half_v1v2_origin = create_v2f(-half_v1v2_origin.y, half_v1v2_origin.x);
        ortho_half_v1v2_origin = normalize_v2f(ortho_half_v1v2_origin);
        ortho_half_v1v2_origin = scale_v2f(ortho_half_v1v2_origin, 2.2f);
        middle_point = add_v2f(middle_point, ortho_half_v1v2_origin);

        // iterate over bezier curve
        v2f last_T = v1;
        for (int i = 1; i <= 20; i++) {
            float t = i / 20.0f;
            v2f aux1 = scale_v2f(v1, (1 - t) * (1 - t));
            v2f aux2 = scale_v2f(middle_point, 2 * (1 - t) * t);
            v2f aux3 = scale_v2f(v2, t * t);
            v2f T = add_v2f(add_v2f(aux1, aux2), aux3);

            GLfloat line_vertices[3 * 2] = {
                last_T.x, last_T.y, 0.2,
                T.x, T.y, 0.2,
            };
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            // NOTE: OpenGL hack (Buffer Object Streaming) to improve performance
            glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), NULL, GL_STREAM_DRAW);
            glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
            glDrawArrays(GL_LINES, 0, 2);
            glBindVertexArray(0);

            last_T = T;
        }

        float t = 0.5f;
        v2f aux1 = scale_v2f(v1, (1 - t) * (1 - t));
        v2f aux2 = scale_v2f(middle_point, 2 * (1 - t) * t);
        v2f aux3 = scale_v2f(v2, t * t);
        middle_point_on_curve = add_v2f(add_v2f(aux1, aux2), aux3);
    } else {
        GLfloat line_vertices[3 * 2] = {
            v1.x, v1.y, 0.2,
            v2.x, v2.y, 0.2,
        };
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        // NOTE: OpenGL hack (Buffer Object Streaming) to improve performance
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), NULL, GL_STREAM_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STREAM_DRAW);
        glDrawArrays(GL_LINES, 0, 2);
        glBindVertexArray(0);
    }

    // draw arrow head

    double magv2v1 = magnitude_v2f(sub_v2f(v1, v2));
    if (magv2v1 > 0.001f) {
        v2f v2v1 = sub_v2f(v1, v2);
        v2f middle_v2v1 = scale_v2f(v2v1, 0.5f);

        v2f normalized_v2v1 = normalize_v2f(v2v1);
        v2f arrow_head_vector = scale_v2f(normalized_v2v1, ARROW_HEAD_CONSTANT / global_state->zoom);

        if (!curved) {
            middle_point_on_curve = add_v2f(v2, middle_v2v1);
        }

        v2f p1, p2, p3;
        p1 = add_v2f(middle_point_on_curve, scale_v2f(arrow_head_vector, -0.5));
        
        p2 = add_v2f(middle_point_on_curve,
                     scale_v2f(create_v2f(-arrow_head_vector.y,
                                          arrow_head_vector.x),
                               0.5));
        p2 = add_v2f(p2, scale_v2f(arrow_head_vector, 0.5));

        p3 = add_v2f(middle_point_on_curve,
                     scale_v2f(create_v2f(arrow_head_vector.y,
                                          -arrow_head_vector.x),
                               0.5));
        p3 = add_v2f(p3, scale_v2f(arrow_head_vector, 0.5));

        GLfloat arrow_vertices[3 * 3] = {
            p2.x, p2.y, 0.2,
            p1.x, p1.y, 0.2,
            p3.x, p3.y, 0.2,
        };
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        // NOTE: OpenGL hack (Buffer Object Streaming) to improve performance
        glBufferData(GL_ARRAY_BUFFER, sizeof(arrow_vertices), NULL, GL_STREAM_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(arrow_vertices), arrow_vertices, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

    // draw edge weight

    if (edge) {
        v2f edge_weight_pos;

        v1.x = (v1.x * global_state->zoom + 1.0f) * (DEFAULT_SCREEN_WIDTH / 2);
        v1.y = (-v1.y * ASPECT_RATIO * global_state->zoom + 1.0f) * (DEFAULT_SCREEN_HEIGHT / 2);
        v2.x = (v2.x * global_state->zoom + 1.0f) * (DEFAULT_SCREEN_WIDTH / 2);
        v2.y = (-v2.y * ASPECT_RATIO * global_state->zoom + 1.0f) * (DEFAULT_SCREEN_HEIGHT / 2);
        v2f temp_v = scale_v2f(sub_v2f(v2, v1), 0.5f);
        v2f v3 = scale_v2f(normalize_v2f(create_v2f(-temp_v.y, temp_v.x)), FONT_SIZE*1.1f);

        if (curved) {
            edge_weight_pos.x = (middle_point_on_curve.x * global_state->zoom + 1.0f) * (DEFAULT_SCREEN_WIDTH / 2);
            edge_weight_pos.y = (-middle_point_on_curve.y * ASPECT_RATIO * global_state->zoom + 1.0f) * (DEFAULT_SCREEN_HEIGHT / 2);
            edge_weight_pos = add_v2f(edge_weight_pos, scale_v2f(v3, -1));
        } else {
            v2f v4 = add_v2f(v1, temp_v);
            edge_weight_pos = add_v2f(v3, v4);
        }

        edge->weight_pos_screen = edge_weight_pos;

        char w[10];
        sprintf(w, "%d", edge->weight);

        font_render_text_horrible(program_font, global_state->font_vbo, global_state->font_vao, cdata,
                                  ftex, edge_weight_pos.x, edge_weight_pos.y, w,
                                  weight_color_r, weight_color_g, weight_color_b, true);
    }
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
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    GLFWwindow *window = glfwCreateWindow(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, "Graphs", NULL, NULL);
    if (!window) {
        force_quit("Could not initialize GLFW");
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
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
    glLineWidth(LINE_WIDTH);

#if 1
    // debug
    int samples;
    glGetIntegerv(GL_SAMPLES, &samples);
    printf("Samples: %d\n", samples);
    glGetIntegerv(GL_MAJOR_VERSION, &samples);
    printf("Major: %d\n", samples);
    glGetIntegerv(GL_MINOR_VERSION, &samples);
    printf("Minor: %d\n", samples);
    glGetIntegerv(GL_MAX_UNIFORM_LOCATIONS, &samples);
    printf("Max uniform locations: %d\n", samples);
#endif


    // shader initialization

    GLuint shader_program = initialize_shader("vertexshader.glsl", "fragshader.glsl");
    GLuint font_shader_program = initialize_shader("font_vertexshader.glsl", "font_fragshader.glsl");

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
    global_state.editing_circle = -1;
    global_state.editing_edge = NULL;
    global_state.showing_menu = true;
    //global_state.temp_weight_str; // NOTE: no need to initialize this
    // DEBUG: add some circles just for testing purposes
    create_vertex(&global_state, create_v2f(1.2, -2.6), 1);
    create_vertex(&global_state, create_v2f(-6.4, -1.1), 1);
    create_vertex(&global_state, create_v2f(-4.1, -4.0), 1);

    glfwSetWindowUserPointer(window, (void *) &global_state);


    // buffers initialization

    // vertices buffers
    {
        GLuint VBO;
        glGenBuffers(1, &VBO);
        glGenVertexArrays(1, &global_state.default_vao);

        GLfloat circle_vertices[NUM_SECTIONS_CIRCLE * 3] = {0};
        for (int i = 0; i < NUM_SECTIONS_CIRCLE; i++) {
            float angle = i * (2 * M_PI / NUM_SECTIONS_CIRCLE);
            circle_vertices[i * 3] = cos(angle);
            circle_vertices[i * 3 + 1] = sin(angle);
            circle_vertices[i * 3 + 2] = 0.0f;
        }

        // TODO: check for error
        glBindVertexArray(global_state.default_vao);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(circle_vertices), circle_vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void *) 0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // edge buffers
    {
        glGenBuffers(1, &global_state.edge_vbo);
        glGenVertexArrays(1, &global_state.edge_vao);

        glBindVertexArray(global_state.edge_vao);
        glBindBuffer(GL_ARRAY_BUFFER, global_state.edge_vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void *) 0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // font buffers
    {
        glGenBuffers(1, &global_state.font_vbo);
        glGenVertexArrays(1, &global_state.font_vao);

        glBindVertexArray(global_state.font_vao);
        glBindBuffer(GL_ARRAY_BUFFER, global_state.font_vbo);
        // NOTE: currently we are uploading data to the font_vbo every frame
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void *) 0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void *) (3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    // menu background buffers
    {
        GLuint VBO;
        glGenBuffers(1, &VBO);
        glGenVertexArrays(1, &global_state.menu_vao);

        float background[6 * 3] = {
            DEFAULT_SCREEN_WIDTH - 500, - 70, 0.5,
            DEFAULT_SCREEN_WIDTH - 5, - 70, 0.5,
            DEFAULT_SCREEN_WIDTH - 500, - 370, 0.5,
            DEFAULT_SCREEN_WIDTH - 500, - 370, 0.5,
            DEFAULT_SCREEN_WIDTH - 5, - 370, 0.5,
            DEFAULT_SCREEN_WIDTH - 5, - 70, 0.5,
        };

        glBindVertexArray(global_state.menu_vao);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(background), background, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void *) 0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
    
    // initialize program data

    GLint scale_uniform = glGetUniformLocation(shader_program, "scale");
    GLint aspect_ratio_uniform = glGetUniformLocation(shader_program, "aspect_ratio");
    GLint translation_uniform = glGetUniformLocation(shader_program, "translation");
    GLint color_uniform = glGetUniformLocation(shader_program, "color");
    GLint filled_uniform = glGetUniformLocation(shader_program, "filled");
    GLint fill_entrance_uniform = glGetUniformLocation(shader_program, "fill_entrance");
    GLint fill_radius_uniform = glGetUniformLocation(shader_program, "fill_radius");
    GLint num_entrances_uniform = glGetUniformLocation(shader_program, "num_entrances");
    GLint font_tex_uniform = glGetUniformLocation(font_shader_program, "tex");


    // initialize font data
    stbtt_bakedchar cdata[96]; // ASCII alphanumeric range
    GLuint ftex;
    glGenTextures(1, &ftex);
    {
        unsigned char *ttf_buffer = malloc((1<<20) * sizeof(*ttf_buffer));
        unsigned char temp_bitmap[512*521];
        FILE *f = fopen("arial.ttf", "rb");
        fread(ttf_buffer, 1, 1<<20, f);
        stbtt_BakeFontBitmap(ttf_buffer, 0, FONT_SIZE, temp_bitmap, 512, 512, 32, 96, cdata);
        glBindTexture(GL_TEXTURE_2D, ftex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, temp_bitmap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        free(ttf_buffer);
        fclose(f);
    }

    double last_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        //glfwWaitEvents();

        double current_time = glfwGetTime();
        global_state.delta_time = current_time - last_time;
        last_time = current_time;

        assert(global_state.zoom > 0);

        v2f current_mouse;
        glfwGetCursorPos(window, &current_mouse.x, &current_mouse.y);
        if (global_state.dragging_map) {
            v2f last = get_untranslated_world_space(window, global_state.zoom, current_mouse);
            v2f now = get_untranslated_world_space(window, global_state.zoom, global_state.last_mouse);
            global_state.cur_translation = scale_v2f(sub_v2f(now, last), -1);
        }
        // TODO; make sure this works if currently also dragging map (does it need to?)
        // TODO: make it possible to select and drag multiple vertices simultaneously
        if (global_state.dragging_vertex) {
            v2f temp = get_cursor_world_space(window, global_state.last_translation, global_state.zoom);
            for (int i = 0; i < global_state.num_circles; i++) {
                if (global_state.circles[i].selected) {
                    global_state.circles[i].pos.x = temp.x;
                    global_state.circles[i].pos.y = temp.y;
                }
            }
        }

        glClearColor(BACKGROUND_COLOR, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader_program);
        glUniform1f(scale_uniform, global_state.zoom);
        glUniform1f(aspect_ratio_uniform, ASPECT_RATIO);

        // screen position
        v2f frame_translation;
        frame_translation.x = global_state.last_translation.x + global_state.cur_translation.x;
        frame_translation.y = global_state.last_translation.y + global_state.cur_translation.y;

        // draw vertices
        {
            vertex_t *circles = global_state.circles;
            float fill_radius_step = 1.0f * global_state.delta_time;

            for (int i = 0; i < global_state.num_circles; i++) {
                glUseProgram(shader_program);
                v2f v = add_v2f(frame_translation, circles[i].pos);
                glUniform3f(translation_uniform, v.x, v.y, 0.0f);
                glUniform1i(filled_uniform, circles[i].filled);
                glUniform1i(num_entrances_uniform, circles[i].num_fill_entrances);
                if (circles[i].filled > 0) {
                    if (global_state.current_animation_root == i) {
                        circles[i].fill_radius[0] = min(circles[i].fill_radius[0] + fill_radius_step, 1.1f /* radius */);
                        if (circles[i].fill_radius[0] > 1.0f /* radius */) {
                            circles[i].filled = 2;
                        }
                        GLfloat t[2] = {v.x, v.y};
                        glUniform2fv(fill_entrance_uniform, 1, t);
                        glUniform1fv(fill_radius_uniform, 1, circles[i].fill_radius);
                    } else {
                        GLfloat fill_entrances[MAX_VERTEX_ENTRANCES * 2] = {0};
                        int aux_count = 0;
                        for (int j = 0; j < circles[i].num_fill_entrances; j++) {
                            vertex_t predecessor = circles[circles[i].fill_entrance_index[j]];
                            if (predecessor.filled == 2) {
                                circles[i].fill_radius[j] = min(circles[i].fill_radius[j] + fill_radius_step, 2.1f /* radius */);
                                if (circles[i].fill_radius[j] > 2.0f /* radius * 2 */) {
                                    circles[i].filled = 2;
                                }
                            }
                            v2f fill_entrance = sub_v2f(circles[i].pos, predecessor.pos);
                            fill_entrance = add_v2f(fill_entrance, scale_v2f(normalize_v2f(fill_entrance), -1.0f /*radius*/));
                            fill_entrance = add_v2f(fill_entrance, predecessor.pos);
                            fill_entrance = add_v2f(frame_translation, fill_entrance);
                            fill_entrances[aux_count++] = fill_entrance.x;
                            fill_entrances[aux_count++] = fill_entrance.y;
                        }
                        glUniform2fv(fill_entrance_uniform, circles[i].num_fill_entrances, fill_entrances);
                        glUniform1fv(fill_radius_uniform, circles[i].num_fill_entrances, circles[i].fill_radius);
                    }
                }
                if (circles[i].filled) {
                    glUniform3f(color_uniform, VERTEX_FILLED_COLOR);
                } else if (circles[i].selected) { // TODO: maybe remove/rethink this whole selected concept
                    glUniform3f(color_uniform, VERTEX_SELECTED_COLOR);
                } else {
                    glUniform3f(color_uniform, VERTEX_DEFAULT_COLOR);
                }
                glBindVertexArray(global_state.default_vao);
                glDrawArrays(GL_TRIANGLE_FAN, 0, NUM_SECTIONS_CIRCLE);
                glBindVertexArray(0);

                // draw vertex weight
                {
                    v.x = (v.x * global_state.zoom + 1.0f) * (DEFAULT_SCREEN_WIDTH / 2);
                    v.y = (-v.y * ASPECT_RATIO * global_state.zoom + 1.0f) * (DEFAULT_SCREEN_HEIGHT / 2);
                    char str[10];
                    sprintf(str, "%d", circles[i].weight);

                    if (global_state.editing_circle == i) {
                        font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                                  ftex, v.x, v.y, str, WEIGHT_EDITING_COLOR, true);
                    } else {
                        font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                                  ftex, v.x, v.y, str, 0, 0, 0, true);
                    }
                }
            }
        }

        // draw edges
        {
            // TODO: clean up all of this, this should all probably be inside the draw_edge call

            // vertices children
            for (int i = 0; i < global_state.num_circles; i++) {
                for (int j = 0; j < global_state.circles[i].num_children; j++) {
                    edge_t *edge = &global_state.circles[i].children[j];
                    int dest = edge->dest;

                    glUseProgram(shader_program);
                    glUniform3f(translation_uniform, 0, 0, 0);
                    glUniform1i(filled_uniform, 0);
                    glUniform2f(fill_entrance_uniform, 0, 0);
                    glUniform1f(fill_radius_uniform, 0);

                    if (global_state.circles[i].filled) {
                        glUniform3f(color_uniform, ARROW_FILLED_COLOR);
                    } else {
                        glUniform3f(color_uniform, ARROW_DEFAULT_COLOR);
                    }

                    v2f v1 = add_v2f(frame_translation, global_state.circles[i].pos);
                    v2f v2 = add_v2f(frame_translation, global_state.circles[dest].pos);

                    // TODO: optimize this by ordering children by index and using binary search when needed (all over the program)
                    bool straight = true;
                    for (int k = 0; k < global_state.circles[dest].num_children; k++) {
                        if (global_state.circles[dest].children[k].dest == i) {
                            straight = false;
                            break;
                        }
                    }

                    draw_edge(edge, global_state.edge_vbo, global_state.edge_vao, global_state.font_vbo, global_state.font_vao,
                              &global_state, v1, v2, !straight, 0, 0, 0, shader_program, font_shader_program, cdata, ftex);
                }
            }

            glUseProgram(shader_program);
            glUniform3f(translation_uniform, 0, 0, 0);
            // TODO: think about this
            glUniform1i(filled_uniform, 0);
            glUniform2f(fill_entrance_uniform, 0, 0);
            glUniform1f(fill_radius_uniform, 0);
            glUniform3f(color_uniform, ARROW_DEFAULT_COLOR);
            // edge being currently created
            if (global_state.modifying_vertex != -1) {
                v2f v1 = add_v2f(frame_translation, global_state.circles[global_state.modifying_vertex].pos);
                v2f v2 = get_cursor_untranslated_world_space(window, global_state.zoom);
                draw_edge(NULL, global_state.edge_vbo, global_state.edge_vao, global_state.font_vbo, global_state.font_vao,
                          &global_state, v1, v2, false, 0, 0, 0, shader_program, font_shader_program, cdata, ftex);
            }
        }

        // draw help menu
        if (global_state.showing_menu) {
            glDisable(GL_DEPTH_TEST);
            glUseProgram(shader_program);
            glUniform3f(translation_uniform, -DEFAULT_SCREEN_WIDTH/2, DEFAULT_SCREEN_HEIGHT/2, 0);
            glUniform1i(filled_uniform, 0);
            glUniform2f(fill_entrance_uniform, 0, 0);
            glUniform1f(fill_radius_uniform, 0);
            glUniform1f(scale_uniform, 1/(DEFAULT_SCREEN_WIDTH/2.0f));
            glUniform3f(color_uniform, 0.7f, 0.7f, 0.7f);

            glBindVertexArray(global_state.menu_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);

            v2f pos = create_v2f(DEFAULT_SCREEN_WIDTH - 500, 100);
            float line_height = FONT_SIZE + 1.0f;
            int line_count = 0;
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "      Comandos:", 0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++), "", 0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  A               Adiciona um vertice", 0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  D               Deleta um vertice", 0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  C               Completa o grafo com arestas de valor 1",
                                      0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  R               Randomiza todos os pesos do grafo", 0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  CTRL        Arraste para adicionar uma aresta", 0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  X               Altera o peso de um vertice/aresta", 0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  B               Executa um BFS comecando no vertice do cursor",
                                      0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  SCROLL   Zoom", 0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  MOUSE2  Arrastar a tela", 0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  E               Exportar para arquivo", 0, 0, 0, false);
            font_render_text_horrible(font_shader_program, global_state.font_vbo, global_state.font_vao, cdata,
                                      ftex, pos.x, pos.y + line_height * (line_count++),
                                      "  TAB          Esconde esse menu", 0, 0, 0, false);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();

    return 0;
}

#version 330

uniform vec3 translation;
uniform float scale;
uniform float aspect_ratio;

layout(location = 0) in vec3 position;

out vec4 out_position;

void main() {
    vec3 temp = translation + position;
    vec3 pos = scale * temp;
    vec3 screen_pos = vec3(pos.x, pos.y * aspect_ratio, pos.z);
    gl_Position = vec4(screen_pos, 1.0);

    out_position = vec4(temp, 1.0);
}


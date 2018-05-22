#version 330

layout(location = 0) in vec3 position;

uniform vec3 translation;
uniform float scale;
uniform float aspect_ratio;

void main() {
    vec3 temp = translation + position;
    //vec3 pos = vec3(vec3(scale * temp).xy, temp.z);
    vec3 pos = scale * temp;
    vec3 screen_pos = vec3(pos.x, pos.y * aspect_ratio, pos.z);
    gl_Position = vec4(screen_pos, 1.0);
}


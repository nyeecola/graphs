#version 330

uniform float lesser_window_side_size;

layout(location = 0) in vec3 vert;
layout(location = 1) in vec2 vertTexCoord;

out vec2 fragTexCoord;

void main() {
    fragTexCoord = vertTexCoord;
    
    gl_Position = vec4((vert / lesser_window_side_size) - vec3(1, -1, 0), 1);
}

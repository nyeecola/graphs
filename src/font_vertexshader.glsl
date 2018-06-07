#version 330

uniform float window_width;
uniform float window_height;

layout(location = 0) in vec3 vert;
layout(location = 1) in vec2 vertTexCoord;

out vec2 fragTexCoord;

void main() {
    fragTexCoord = vertTexCoord;
    
    vec3 v = vec3(vert.x / (window_width/2), vert.y / (window_height/2), vert.z);
    gl_Position = vec4(v, 1);
}

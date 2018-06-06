#version 330

uniform sampler2D tex;
uniform vec3 color;

in vec2 fragTexCoord;

layout(location = 0) out vec4 finalColor;

void main() {
    finalColor = vec4(color, texture(tex, fragTexCoord).r);
}

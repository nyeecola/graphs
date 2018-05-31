#version 330

uniform vec3 color;
uniform bool filled;
uniform vec2 fill_entrance;
uniform float fill_radius;

in vec4 out_position;

layout(location = 0) out vec4 frag_color;

void main() {
    // TODO: maybe pass this as an uniform?
    vec3 vertex_default_color = vec3(0.8, 0.8, 0.8);
    if (filled) {
        float dist = distance(vec4(fill_entrance, 0, out_position.w), out_position);
        float delta = 0.1;
        float lerp_aux = smoothstep(fill_radius-delta, fill_radius+delta, dist);
        //frag_color = vec4(dist * color, 1.0);
        frag_color = vec4(mix(color, vertex_default_color, lerp_aux), 1.0);
    } else {
        frag_color = vec4(color, 1.0);
    }
}

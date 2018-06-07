#version 330

uniform vec3 color;
uniform bool filled;
uniform vec2 fill_entrance[100]; // TODO: change this to a constant or something
uniform float fill_radius[100]; // TODO: change this to a constant or something
uniform int num_entrances;

in vec4 out_position;

layout(location = 0) out vec4 frag_color;

void main() {
    // TODO: maybe pass this as an uniform?
    vec3 vertex_default_color = vec3(0.8, 0.8, 0.8);
    if (filled) {
        float delta = 0.18; // TODO: change this to a constant or something
        vec3 final_color = vertex_default_color;
#if 0
        float lowest_dist = -1;
        int nearest_entrance = -1;
        for (int i = 0; i < num_entrances; i++) {
            float dist = distance(vec4(fill_entrance[i], 0, out_position.w), out_position);
            if ((dist < lowest_dist && fill_radius[i]+delta >= dist) || lowest_dist < 0) {
                lowest_dist = dist;
                nearest_entrance = i;
            }
        }
        float lerp_aux = smoothstep(fill_radius[nearest_entrance]-delta,
                                    fill_radius[nearest_entrance]+delta,
                                    lowest_dist);
        frag_color = vec4(mix(color, vertex_default_color, lerp_aux), 1.0);
#else
        for (int i = 0; i < num_entrances; i++) {
            float dist = distance(vec4(fill_entrance[i], 0, out_position.w), out_position);
            float lerp_aux = smoothstep(fill_radius[i]-delta, fill_radius[i]+delta, dist);
            vec3 temp_color = mix(color, vertex_default_color, lerp_aux);
            vec3 color_offset = abs(vertex_default_color - temp_color);
            final_color = clamp(final_color - color_offset, color, vertex_default_color);
        }
        frag_color = vec4(final_color, 1.0);
#endif
    } else {
        frag_color = vec4(color, 1.0);
    }
}

#version 330
#extension GL_ARB_explicit_uniform_location : require


layout(location = 2) uniform vec3 size;
layout(location = 3) uniform vec3 offset;
layout(location = 4) uniform vec4 in_color;
layout(location = 5) uniform int isGrid;
layout(location = 6) uniform int progress;

const float distedge = 0.5;
const float feather = 0.5;

const float boxalpha = 0.2;

in vec4 vertex_pos;
out vec4 color;

float edge(float dist)
{
    if (dist == 0.0) {
        return boxalpha;
    }
    if (dist < distedge) {
        return 1.0;
    }
    if (dist > distedge + feather)
    {
        return boxalpha;
    }
    float ret = (dist - distedge) / feather; // mapped to 0..1
    ret *= boxalpha - 1; // 0..boxalpha-1
    ret = 1.0 + ret; // 1..boxalpha
    return ret;
}

float grid(float dist)
{
    if (dist == 0.0) {
        return boxalpha;
    }
    if (mod(dist,100.0) > 1.0) {
        return boxalpha;
    }
    return 1.0;
}

void main() {
    color = in_color;
    //color.a = 0.2

    if (isGrid == 1) {
        float edgealpha = boxalpha;
        color = vec4(0.1,0.1,0.1,1.0);
        const float divisor = 765 / 8.0;
        if (mod(abs(vertex_pos.x), divisor) < 5.0 ||
            (vertex_pos.y > 0.0 && mod(abs(vertex_pos.y), divisor) < 5.0) ||
            (mod(abs(vertex_pos.z), divisor) < 5.0)) {
            color = vec4(0.3,0.3,0.3,1.0);
        }
        if (abs(vertex_pos.x) < 1.0 || abs(vertex_pos.z) < 1.0) {
            color = vec4(0.9,0.3,0.2,1.0);
        }
        //color.a = edgealpha;
        return;
    }

    if (isGrid == 2) {
        float distFromCenter = length(vertex_pos.xyz - (offset + size/2));
        if (distFromCenter < progress) {
            color = in_color;
        } else {
            color = vec4(0.0,0.0,0.0,0.0);
        }
        return;
    }

    // edge
    float edgealpha = boxalpha;
    edgealpha = max(edgealpha, edge(abs(vertex_pos.x - offset.x)));
    edgealpha = max(edgealpha, edge(abs(vertex_pos.x - (offset.x+size.x))));
    edgealpha = max(edgealpha, edge(abs(vertex_pos.y - offset.y)));
    edgealpha = max(edgealpha, edge(abs(vertex_pos.y - (offset.y+size.y))));
    edgealpha = max(edgealpha, edge(abs(vertex_pos.z - offset.z)));
    edgealpha = max(edgealpha, edge( abs(vertex_pos.z - (offset.z+size.z))));

    color.a = edgealpha;
}
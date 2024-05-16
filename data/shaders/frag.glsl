#version 330
#extension GL_ARB_explicit_uniform_location : require


layout(location = 2) uniform vec2 size;
layout(location = 3) uniform vec2 offset;
layout(location = 4) uniform vec4 in_color;

const float distedge = 1.5;

in vec4 vertex_pos;
out vec4 color;

void main() {
    color = in_color;

    // edge
    if (abs(vertex_pos.x - offset.x) > distedge &&
        abs(vertex_pos.x - (offset.x+size.x)) > distedge &&
        abs(vertex_pos.y - offset.y) > distedge &&
        abs(vertex_pos.y - (offset.y+size.y)) > distedge) 
    {
        color.a = 0.2;
    }
}
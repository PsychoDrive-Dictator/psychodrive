#version 330
#extension GL_ARB_explicit_uniform_location : require

layout(location = 0) in vec3 in_pos;

out vec4 vertex_pos;

layout(location = 0) uniform mat4 view;
layout(location = 1) uniform mat4 proj;

layout(location = 2) uniform vec3 size;
layout(location = 3) uniform vec3 offset;

void main() {
    vec4 pos = vec4(in_pos, 1.0) * vec4(size, 1.0) + vec4(offset, 0.0);
    vertex_pos = pos;
    gl_Position = proj * view * pos;
};
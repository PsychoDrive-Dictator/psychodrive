precision mediump float;

in vec3 in_pos;

out vec4 vertex_pos;

uniform mat4 view;
uniform mat4 proj;

uniform vec3 size;
uniform vec3 offset;

void main() {
    vec4 pos = vec4(in_pos, 1.0) * vec4(size, 1.0) + vec4(offset, 0.0);
    vertex_pos = pos;
    gl_Position = proj * view * pos;
}

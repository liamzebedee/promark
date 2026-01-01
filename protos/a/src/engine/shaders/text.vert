attribute vec2 a_position;
attribute vec2 a_texcoord;
attribute vec4 a_color;

uniform mat4 u_projection;

varying vec2 v_texcoord;
varying vec4 v_color;

void main() {
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
    v_color = a_color;
}

varying vec2 v_texcoord;
varying vec4 v_color;

uniform sampler2D u_texture;

void main() {
    vec4 texColor = texture2D(u_texture, v_texcoord);
    gl_FragColor = texColor * v_color;
}

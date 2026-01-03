// Auto-generated shader sources - do not edit manually
// Generated from src/engine/shaders/*.vert and *.frag
#pragma once

namespace Shaders {

const char* TEXT_VERT = R"(
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
)";

const char* TEXT_FRAG = R"(
varying vec2 v_texcoord;
varying vec4 v_color;

uniform sampler2D u_texture;

void main() {
    float alpha = texture2D(u_texture, v_texcoord).a;
    gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
)";

const char* SOLID_VERT = R"(
attribute vec2 a_position;
attribute vec4 a_color;

uniform mat4 u_projection;

varying vec4 v_color;

void main() {
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_color = a_color;
}
)";

const char* SOLID_FRAG = R"(
varying vec4 v_color;

void main() {
    gl_FragColor = v_color;
}
)";

const char* IMAGE_FRAG = R"(
varying vec2 v_texcoord;
varying vec4 v_color;

uniform sampler2D u_texture;

void main() {
    vec4 texColor = texture2D(u_texture, v_texcoord);
    gl_FragColor = texColor * v_color;
}
)";

} // namespace Shaders

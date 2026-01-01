#include "batch_renderer.h"
#include "utf8.h"
#include <OpenGL/gl.h>
#include <cstring>
#include <iostream>

// GLSL 1.20 shaders (OpenGL 2.1 compatible)
static const char* textVertexShader = R"(
#version 120
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

static const char* textFragmentShader = R"(
#version 120
varying vec2 v_texcoord;
varying vec4 v_color;

uniform sampler2D u_texture;

void main() {
    float alpha = texture2D(u_texture, v_texcoord).a;
    gl_FragColor = vec4(v_color.rgb, v_color.a * alpha);
}
)";

static const char* solidVertexShader = R"(
#version 120
attribute vec2 a_position;
attribute vec4 a_color;

uniform mat4 u_projection;

varying vec4 v_color;

void main() {
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_color = a_color;
}
)";

static const char* solidFragmentShader = R"(
#version 120
varying vec4 v_color;

void main() {
    gl_FragColor = v_color;
}
)";

BatchRenderer::BatchRenderer()
    : textProg(0), solidProg(0), vbo(0), viewportW(800), viewportH(600),
      glyphAtlas(nullptr), textured(false) {
    memset(projMatrix, 0, sizeof(projMatrix));
}

BatchRenderer::~BatchRenderer() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (textProg) glDeleteProgram(textProg);
    if (solidProg) glDeleteProgram(solidProg);
}

bool BatchRenderer::init() {
    // Compile shaders with attribute bindings
    // Text shader
    unsigned int tvs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(tvs, 1, &textVertexShader, nullptr);
    glCompileShader(tvs);

    unsigned int tfs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(tfs, 1, &textFragmentShader, nullptr);
    glCompileShader(tfs);

    unsigned int tprog = glCreateProgram();
    glBindAttribLocation(tprog, 0, "a_position");
    glBindAttribLocation(tprog, 1, "a_texcoord");
    glBindAttribLocation(tprog, 2, "a_color");
    glAttachShader(tprog, tvs);
    glAttachShader(tprog, tfs);
    glLinkProgram(tprog);
    glDeleteShader(tvs);
    glDeleteShader(tfs);

    // Solid shader
    unsigned int svs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(svs, 1, &solidVertexShader, nullptr);
    glCompileShader(svs);

    unsigned int sfs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sfs, 1, &solidFragmentShader, nullptr);
    glCompileShader(sfs);

    unsigned int sprog = glCreateProgram();
    glBindAttribLocation(sprog, 0, "a_position");
    glBindAttribLocation(sprog, 2, "a_color");
    glAttachShader(sprog, svs);
    glAttachShader(sprog, sfs);
    glLinkProgram(sprog);
    glDeleteShader(svs);
    glDeleteShader(sfs);

    textProg = tprog;
    solidProg = sprog;

    // Create VBO
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    vertices.reserve(MAX_VERTICES);
    return true;
}

void BatchRenderer::setViewport(int width, int height, float scrollOffsetY) {
    viewportW = width;
    viewportH = height;

    // Orthographic projection (top-left origin)
    float l = 0, r = (float)width, t = 0, b = (float)height;
    float n = -1, f = 1;

    memset(projMatrix, 0, sizeof(projMatrix));
    projMatrix[0] = 2.0f / (r - l);
    projMatrix[5] = 2.0f / (t - b);  // Flip Y
    projMatrix[10] = -2.0f / (f - n);
    projMatrix[12] = -(r + l) / (r - l);
    projMatrix[13] = -(t + b) / (t - b);
    projMatrix[14] = -(f + n) / (f - n);
    projMatrix[15] = 1.0f;

    // Apply vertical translation for scroll offset
    // scrollOffsetY is the Y translation to apply (positive = shift content down)
    projMatrix[13] += scrollOffsetY * projMatrix[5];
}

void BatchRenderer::begin() {
    vertices.clear();
    textured = false;
}

void BatchRenderer::flush() {
    if (vertices.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());

    unsigned int prog = textured ? textProg : solidProg;
    glUseProgram(prog);

    // Set projection uniform
    int projLoc = glGetUniformLocation(prog, "u_projection");
    if (projLoc >= 0) glUniformMatrix4fv(projLoc, 1, GL_FALSE, projMatrix);

    if (textured) {
        int texLoc = glGetUniformLocation(prog, "u_texture");
        if (texLoc >= 0) glUniform1i(texLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glyphAtlas->bind();
    }

    // Set up vertex attributes (locations bound during init)
    glEnableVertexAttribArray(0);  // position
    glEnableVertexAttribArray(1);  // texcoord
    glEnableVertexAttribArray(2);  // color

    size_t stride = sizeof(Vertex);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, x));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, u));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, r));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, vertices.size());

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    // Restore state for immediate mode compatibility
    glDisable(GL_BLEND);

    vertices.clear();
}

void BatchRenderer::ensureCapacity(size_t numVertices) {
    if (vertices.size() + numVertices > MAX_VERTICES) {
        flush();
    }
}

void BatchRenderer::drawQuad(float x, float y, float w, float h,
                              float u0, float v0, float u1, float v1,
                              float r, float g, float b, float a) {
    // Flush if switching from solid to textured
    if (!textured && !vertices.empty()) {
        flush();
    }
    textured = true;
    ensureCapacity(6);

    // Two triangles
    Vertex v[6] = {
        {x,     y,     u0, v0, r, g, b, a},
        {x + w, y,     u1, v0, r, g, b, a},
        {x + w, y + h, u1, v1, r, g, b, a},

        {x,     y,     u0, v0, r, g, b, a},
        {x + w, y + h, u1, v1, r, g, b, a},
        {x,     y + h, u0, v1, r, g, b, a},
    };

    vertices.insert(vertices.end(), v, v + 6);
}

void BatchRenderer::drawRect(float x, float y, float w, float h,
                              float r, float g, float b, float a) {
    // Flush if switching from textured to solid
    if (textured && !vertices.empty()) {
        flush();
    }
    textured = false;
    ensureCapacity(6);

    Vertex v[6] = {
        {x,     y,     0, 0, r, g, b, a},
        {x + w, y,     0, 0, r, g, b, a},
        {x + w, y + h, 0, 0, r, g, b, a},

        {x,     y,     0, 0, r, g, b, a},
        {x + w, y + h, 0, 0, r, g, b, a},
        {x,     y + h, 0, 0, r, g, b, a},
    };

    vertices.insert(vertices.end(), v, v + 6);
}

void BatchRenderer::drawImage(float x, float y, float w, float h, unsigned int textureId) {
    // Flush any pending geometry first
    flush();

    // Draw image with its own texture
    glUseProgram(textProg);
    int projLoc = glGetUniformLocation(textProg, "u_projection");
    if (projLoc >= 0) glUniformMatrix4fv(projLoc, 1, GL_FALSE, projMatrix);
    int texLoc = glGetUniformLocation(textProg, "u_texture");
    if (texLoc >= 0) glUniform1i(texLoc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);

    // Build quad vertices
    Vertex v[6] = {
        {x,     y,     0, 0, 1, 1, 1, 1},
        {x + w, y,     1, 0, 1, 1, 1, 1},
        {x + w, y + h, 1, 1, 1, 1, 1, 1},
        {x,     y,     0, 0, 1, 1, 1, 1},
        {x + w, y + h, 1, 1, 1, 1, 1, 1},
        {x,     y + h, 0, 1, 1, 1, 1, 1},
    };

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    size_t stride = sizeof(Vertex);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, x));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, u));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, r));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

void BatchRenderer::drawText(const std::string& text, float x, float y,
                              float r, float g, float b, float a,
                              int fontSize, TextStyle style, bool mono, FT_Face face) {
    if (!glyphAtlas) return;

    float penX = x;
    float baseline = y;

    size_t pos = 0;
    while (pos < text.length()) {
        uint32_t codepoint = utf8::decode(text, pos);
        if (codepoint == '\n') continue;

        const AtlasGlyph* glyph = glyphAtlas->get(codepoint, fontSize, style, mono, face);
        if (!glyph) continue;

        if (glyph->width > 0 && glyph->height > 0) {
            float xpos = penX + glyph->bearingX;
            float ypos = baseline - glyph->bearingY;

            drawQuad(xpos, ypos, glyph->width, glyph->height,
                     glyph->u0, glyph->v0, glyph->u1, glyph->v1,
                     r, g, b, a);
        }

        penX += glyph->advance;
    }
}

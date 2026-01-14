#include "opengl_backend.h"
#include "shaders_embedded.h"
#include "utf8.h"
#include "gl_includes.h"
#include <cstring>
#include <iostream>

OpenGLBackend::OpenGLBackend()
    : initialized(false), viewportW(800), viewportH(600), scrollOffsetY(0),
      textProg(0), solidProg(0), imageProg(0), vbo(0), texturedMode(false),
      atlasTexId(0), atlasW(1024), atlasH(1024),
      shelfY(0), shelfH(0), shelfX(0) {
    memset(projMatrix, 0, sizeof(projMatrix));
}

OpenGLBackend::~OpenGLBackend() {
    // Clean up GL resources
    if (vbo) glDeleteBuffers(1, &vbo);
    if (textProg) glDeleteProgram(textProg);
    if (solidProg) glDeleteProgram(solidProg);
    if (imageProg) glDeleteProgram(imageProg);
    if (atlasTexId) glDeleteTextures(1, &atlasTexId);
}

bool OpenGLBackend::init() {
    if (initialized) return true;

    // Print GL info
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // Set up default GL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifndef __EMSCRIPTEN__
    glEnable(GL_TEXTURE_2D);
#endif

    // Compile shaders
    if (!compileShaders()) {
        std::cerr << "Failed to compile shaders" << std::endl;
        return false;
    }

    // Create VBO
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Initialize glyph atlas
    if (!initAtlas()) {
        std::cerr << "Failed to initialize glyph atlas" << std::endl;
        return false;
    }

    vertices.reserve(MAX_VERTICES);
    initialized = true;

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error during backend initialization: " << error << std::endl;
        return false;
    }

    std::cout << "OpenGLBackend initialized successfully" << std::endl;
    return true;
}

bool OpenGLBackend::initAtlas() {
    glGenTextures(1, &atlasTexId);
    glBindTexture(GL_TEXTURE_2D, atlasTexId);

    // Allocate empty texture (GL_ALPHA for single-channel glyph data)
    std::vector<uint8_t> empty(atlasW * atlasH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlasW, atlasH, 0,
                 GL_ALPHA, GL_UNSIGNED_BYTE, empty.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool OpenGLBackend::compileShaders() {
    // Platform-specific shader preamble
#ifdef __EMSCRIPTEN__
    const char* vertPreamble = "";
    const char* fragPreamble = "precision mediump float;\n";
#else
    const char* vertPreamble = "#version 120\n";
    const char* fragPreamble = "#version 120\n";
#endif

    std::string textVS = std::string(vertPreamble) + Shaders::TEXT_VERT;
    std::string textFS = std::string(fragPreamble) + Shaders::TEXT_FRAG;
    std::string solidVS = std::string(vertPreamble) + Shaders::SOLID_VERT;
    std::string solidFS = std::string(fragPreamble) + Shaders::SOLID_FRAG;
    std::string imageFS = std::string(fragPreamble) + Shaders::IMAGE_FRAG;

    // Text shader (textured quads with alpha from texture)
    unsigned int tvs = compileShader(GL_VERTEX_SHADER, textVS.c_str());
    unsigned int tfs = compileShader(GL_FRAGMENT_SHADER, textFS.c_str());
    textProg = linkProgram(tvs, tfs, "a_position", "a_texcoord", "a_color");
    glDeleteShader(tvs);
    glDeleteShader(tfs);

    // Solid shader (colored rectangles)
    unsigned int svs = compileShader(GL_VERTEX_SHADER, solidVS.c_str());
    unsigned int sfs = compileShader(GL_FRAGMENT_SHADER, solidFS.c_str());
    solidProg = linkProgram(svs, sfs, "a_position", nullptr, "a_color");
    glDeleteShader(svs);
    glDeleteShader(sfs);

    // Image shader (textured quads with RGBA from texture * tint)
    unsigned int ivs = compileShader(GL_VERTEX_SHADER, textVS.c_str());  // Reuse text vertex shader
    unsigned int ifs = compileShader(GL_FRAGMENT_SHADER, imageFS.c_str());
    imageProg = linkProgram(ivs, ifs, "a_position", "a_texcoord", "a_color");
    glDeleteShader(ivs);
    glDeleteShader(ifs);

    return textProg != 0 && solidProg != 0 && imageProg != 0;
}

unsigned int OpenGLBackend::compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // Check compilation status
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        return 0;
    }

    return shader;
}

unsigned int OpenGLBackend::linkProgram(unsigned int vertShader, unsigned int fragShader,
                                         const char* posAttr, const char* texAttr, const char* colorAttr) {
    unsigned int program = glCreateProgram();
    glBindAttribLocation(program, 0, posAttr);
    if (texAttr) glBindAttribLocation(program, 1, texAttr);
    glBindAttribLocation(program, 2, colorAttr);
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    // Check link status
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader link failed: " << infoLog << std::endl;
        return 0;
    }

    return program;
}

void OpenGLBackend::beginFrame(int width, int height) {
    viewportW = width;
    viewportH = height;
    updateProjection();
    vertices.clear();
    texturedMode = false;
}

void OpenGLBackend::endFrame() {
    flush();
}

void OpenGLBackend::setViewport(int x, int y, int width, int height) {
    flush();  // Flush before changing viewport
    glViewport(x, y, width, height);
    viewportW = width;
    viewportH = height;
    updateProjection();
}

void OpenGLBackend::clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLBackend::pushClip(int x, int y, int width, int height) {
    flush();  // Flush before changing scissor

    // Save current clip state
    ClipRect clip = {x, y, width, height};
    clipStack.push(clip);

    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
}

void OpenGLBackend::popClip() {
    flush();  // Flush before changing scissor

    if (!clipStack.empty()) {
        clipStack.pop();
    }

    if (clipStack.empty()) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        const ClipRect& clip = clipStack.top();
        glScissor(clip.x, clip.y, clip.width, clip.height);
    }
}

void OpenGLBackend::setScrollOffset(float offsetY) {
    if (scrollOffsetY != offsetY) {
        flush();  // Flush before changing projection
        scrollOffsetY = offsetY;
        updateProjection();
    }
}

void OpenGLBackend::updateProjection() {
    // Orthographic projection (top-left origin)
    float l = 0, r = (float)viewportW, t = 0, b = (float)viewportH;
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
    projMatrix[13] += scrollOffsetY * projMatrix[5];
}

void OpenGLBackend::ensureCapacity(size_t numVertices) {
    if (vertices.size() + numVertices > MAX_VERTICES) {
        flush();
    }
}

void OpenGLBackend::drawRect(float x, float y, float w, float h,
                              float r, float g, float b, float a) {
    // Flush if switching from textured to solid
    if (texturedMode && !vertices.empty()) {
        flush();
    }
    texturedMode = false;
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

void OpenGLBackend::drawLine(float x1, float y1, float x2, float y2,
                              float thickness, float r, float g, float b, float a) {
    // Draw line as a thin rectangle
    float dx = x2 - x1;
    float dy = y2 - y1;

    if (std::abs(dx) > std::abs(dy)) {
        // Horizontal-ish line
        float minX = std::min(x1, x2);
        float y = y1 - thickness / 2.0f;
        drawRect(minX, y, std::abs(dx), thickness, r, g, b, a);
    } else {
        // Vertical-ish line
        float minY = std::min(y1, y2);
        float x = x1 - thickness / 2.0f;
        drawRect(x, minY, thickness, std::abs(dy), r, g, b, a);
    }
}

void OpenGLBackend::drawQuad(float x, float y, float w, float h,
                              float u0, float v0, float u1, float v1,
                              float r, float g, float b, float a) {
    // Flush if switching from solid to textured
    if (!texturedMode && !vertices.empty()) {
        flush();
    }
    texturedMode = true;
    ensureCapacity(6);

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

void OpenGLBackend::drawText(const std::string& text, float x, float y,
                              float r, float g, float b, float a,
                              int fontSize, TextStyle style, bool monospace,
                              FT_Face face) {
    if (!face) return;

    float penX = x;
    float baseline = y;

    size_t pos = 0;
    while (pos < text.length()) {
        uint32_t codepoint = utf8::decode(text, pos);
        if (codepoint == '\n') continue;

        const BackendGlyph* glyph = getGlyph(codepoint, fontSize, style, monospace, face);
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

void OpenGLBackend::drawImage(float x, float y, float w, float h,
                               uint32_t textureId,
                               float srcU0, float srcV0, float srcU1, float srcV1,
                               float tintR, float tintG, float tintB, float tintA) {
    // Flush any pending geometry first (images use different texture)
    flush();

    // Draw image with its own texture using image shader
    glUseProgram(imageProg);
    int projLoc = glGetUniformLocation(imageProg, "u_projection");
    if (projLoc >= 0) glUniformMatrix4fv(projLoc, 1, GL_FALSE, projMatrix);
    int texLoc = glGetUniformLocation(imageProg, "u_texture");
    if (texLoc >= 0) glUniform1i(texLoc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);

    // Build quad vertices
    Vertex v[6] = {
        {x,     y,     srcU0, srcV0, tintR, tintG, tintB, tintA},
        {x + w, y,     srcU1, srcV0, tintR, tintG, tintB, tintA},
        {x + w, y + h, srcU1, srcV1, tintR, tintG, tintB, tintA},
        {x,     y,     srcU0, srcV0, tintR, tintG, tintB, tintA},
        {x + w, y + h, srcU1, srcV1, tintR, tintG, tintB, tintA},
        {x,     y + h, srcU0, srcV1, tintR, tintG, tintB, tintA},
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

uint32_t OpenGLBackend::createTexture(int width, int height, const uint8_t* pixels, bool rgba) {
    uint32_t texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    GLenum format = rgba ? GL_RGBA : GL_ALPHA;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                 format, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    return texId;
}

void OpenGLBackend::deleteTexture(uint32_t textureId) {
    if (textureId != 0) {
        glDeleteTextures(1, &textureId);
    }
}

void OpenGLBackend::flush() {
    if (vertices.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());

    unsigned int prog = texturedMode ? textProg : solidProg;
    glUseProgram(prog);

    // Set projection uniform
    int projLoc = glGetUniformLocation(prog, "u_projection");
    if (projLoc >= 0) glUniformMatrix4fv(projLoc, 1, GL_FALSE, projMatrix);

    if (texturedMode) {
        int texLoc = glGetUniformLocation(prog, "u_texture");
        if (texLoc >= 0) glUniform1i(texLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        bindAtlas();
    }

    // Set up vertex attributes
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

    // Restore state
    glDisable(GL_BLEND);

    vertices.clear();
}

void OpenGLBackend::bindAtlas() {
    glBindTexture(GL_TEXTURE_2D, atlasTexId);
}

const BackendGlyph* OpenGLBackend::getGlyph(uint32_t codepoint, int fontSize,
                                             TextStyle style, bool mono, FT_Face face) {
    BackendGlyphKey key{codepoint, fontSize, static_cast<uint8_t>(style), mono};

    auto it = glyphCache.find(key);
    if (it != glyphCache.end()) {
        return &it->second;
    }

    // Set font size
    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Load and render glyph
    if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER)) {
        return nullptr;
    }

    FT_GlyphSlot g = face->glyph;

    BackendGlyph glyph;
    glyph.width = g->bitmap.width;
    glyph.height = g->bitmap.rows;
    glyph.bearingX = g->bitmap_left;
    glyph.bearingY = g->bitmap_top;
    glyph.advance = g->advance.x >> 6;

    // Add to atlas if it has a bitmap
    if (g->bitmap.width > 0 && g->bitmap.rows > 0) {
        if (!addGlyphToAtlas(g->bitmap.width, g->bitmap.rows, g->bitmap.buffer,
                             glyph.u0, glyph.v0, glyph.u1, glyph.v1)) {
            // Atlas full
            return nullptr;
        }
    } else {
        // Space or empty glyph - no texture needed
        glyph.u0 = glyph.v0 = glyph.u1 = glyph.v1 = 0;
    }

    glyphCache[key] = glyph;
    return &glyphCache[key];
}

bool OpenGLBackend::addGlyphToAtlas(int w, int h, const uint8_t* bitmap,
                                     float& u0, float& v0, float& u1, float& v1) {
    // Check if glyph fits in current shelf
    if (shelfX + w + ATLAS_PAD > atlasW) {
        // Move to next shelf
        shelfY += shelfH + ATLAS_PAD;
        shelfH = 0;
        shelfX = 0;
    }

    // Check if we need a new shelf and it fits
    if (shelfY + h + ATLAS_PAD > atlasH) {
        // Atlas is full
        return false;
    }

    // Update shelf height if this glyph is taller
    if (h > shelfH) {
        shelfH = h;
    }

    // Upload glyph bitmap to atlas
    glBindTexture(GL_TEXTURE_2D, atlasTexId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, shelfX, shelfY, w, h,
                    GL_ALPHA, GL_UNSIGNED_BYTE, bitmap);

    // Calculate UV coordinates
    u0 = (float)shelfX / atlasW;
    v0 = (float)shelfY / atlasH;
    u1 = (float)(shelfX + w) / atlasW;
    v1 = (float)(shelfY + h) / atlasH;

    // Advance position
    shelfX += w + ATLAS_PAD;

    return true;
}

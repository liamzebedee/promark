#pragma once

// OpenGL ES 2.0 style includes for all platforms
#ifdef __EMSCRIPTEN__
#include <GLES2/gl2.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#else
// Linux: desktop GL with ES 2.0 compatible functions
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

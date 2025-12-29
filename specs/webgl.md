1. Install + activate emsdk (once), then build with emcc/em++ (this is the “GLFW shim to WebGL” route: your C++ keeps calling glfw + gl*, Emscripten provides a GLFW3-compatible layer and maps GLES-ish GL calls to WebGL). Emscripten explicitly supports “more limited support for … glfw” in its runtime model. ([emscripten.org][1])

2. Constrain your GL usage to the WebGL-friendly OpenGL ES 2/3 subset (no desktop-only GL). This is the default, recommended mode; you pick WebGL2 by setting MAX_WEBGL_VERSION (and MIN_WEBGL_VERSION to forbid fallback). ([emscripten.org][2])

3. Build flags: link with the built-in GLFW port via -sUSE_GLFW=3 (Emscripten ships its own glfw build; you don’t compile glfw yourself for wasm). ([Gist][3])

4. If you use FreeType, either build it yourself to wasm or use the Emscripten port flag -sUSE_FREETYPE=1 / --use-port=freetype (so headers/libs are provided). ([emscripten.org][4])

5. Main loop: browsers don’t allow a tight while(true) render loop on the main thread; you must yield to the browser via emscripten_set_main_loop (or equivalent). ([emscripten.org][1])

Minimal “keep my GLFW code” scaffold (single-source that works native + web):

```cpp
#include <GLFW/glfw3.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static GLFWwindow* g_window = nullptr;

static void frame() {
  // render here
  // glClear(...); draw...
  glfwSwapBuffers(g_window);
  glfwPollEvents();
#ifdef __EMSCRIPTEN__
  if (glfwWindowShouldClose(g_window)) emscripten_cancel_main_loop();
#endif
}

int main() {
  if (!glfwInit()) return 1;

#ifdef __EMSCRIPTEN__
  // Ask for WebGL2-ish (ES3) context when available; keep it simple.
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#endif

  g_window = glfwCreateWindow(1280, 720, "app", nullptr, nullptr);
  if (!g_window) return 2;
  glfwMakeContextCurrent(g_window);

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(frame, 0, 1); // browser drives frames :contentReference[oaicite:5]{index=5}
#else
  while (!glfwWindowShouldClose(g_window)) frame();
#endif

  glfwDestroyWindow(g_window);
  glfwTerminate();
  return 0;
}
```

One-shot build command (no CMake), assuming your code is already “GLES/WebGL subset clean”:

```bash
em++ src/main.cpp -O3 \
  -sUSE_GLFW=3 \
  -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 \
  -sFULL_ES3=1 \
  -sUSE_FREETYPE=1 \
  --preload-file assets/fonts@/fonts \
  -o dist/index.html
```

Notes on those switches (why they exist, so you don’t cargo-cult):

* -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 forces WebGL2-only builds (smaller + no fallback); Emscripten documents MIN/MAX_WEBGL_VERSION for selecting WebGL2 contexts. ([emscripten.org][2])
* -sFULL_ES3=1 enables ES3 “emulation glue” for features that don’t map 1:1; use only if you actually need those ES3-ish behaviors (otherwise omit). ([emscripten.org][2])
* -sUSE_GLFW=3 is the “GLFW shim” itself. ([Gist][3])
* -sUSE_FREETYPE=1 pulls in the FreeType port. ([emscripten.org][4])

CMake recipe (so you can keep your normal desktop build and add a web target):

```cmake
cmake_minimum_required(VERSION 3.20)
project(app CXX)

add_executable(app src/main.cpp)

if(EMSCRIPTEN)
  target_link_options(app PRIVATE
    -sUSE_GLFW=3
    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
    -sFULL_ES3=1
    -sUSE_FREETYPE=1
    --preload-file ${CMAKE_SOURCE_DIR}/assets/fonts@/fonts
    -o index.html
  )
endif()
```

Build:

```bash
# native
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native -j

# web
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
python3 -m http.server --directory build-web 8000
```

If your current code is desktop-GL (GL_ALPHA textures, legacy enums, etc.), the single most common “why is it black in browser” failure mode is “you’re not actually in the WebGL-friendly GLES subset”. Emscripten’s OpenGL doc is blunt: default mode is the subset that maps directly to WebGL; anything else needs emulation and can break/slow. ([emscripten.org][2])

[1]: https://emscripten.org/docs/porting/emscripten-runtime-environment.html "Emscripten Runtime Environment — Emscripten 4.0.23-git (dev) documentation"
[2]: https://emscripten.org/_sources/docs/porting/multimedia_and_graphics/OpenGL-support.txt "emscripten.org"
[3]: https://gist.github.com/ousttrue/0f3a11d5d28e365b129fe08f18f4e141?utm_source=chatgpt.com "emscripten glfw3 or webgl sample"
[4]: https://emscripten.org/docs/tools_reference/settings_reference.html?utm_source=chatgpt.com "Emscripten Compiler Settings"

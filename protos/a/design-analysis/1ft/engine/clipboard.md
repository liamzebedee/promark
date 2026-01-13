# Clipboard Module Design Analysis

**Files:**
- `src/engine/clipboard.h` (29 lines)
- `src/engine/clipboard.cpp` (51 lines)

---

## 1. Responsibilities

The Clipboard module provides a platform-agnostic interface for system clipboard operations (copy/paste of text). Its stated responsibilities are:

1. **Text retrieval** - Read text from the system clipboard (`getText()`, line 19 of header)
2. **Text storage** - Write text to the system clipboard (`setText()`, line 20 of header)
3. **Platform abstraction** - Allow custom handlers for non-desktop platforms like web (`setHandlers()`, line 13 of header)
4. **Default behavior** - Fall back to GLFW clipboard on desktop (`useDefaultHandlers()`, line 16 of header)

The module is designed as a **static singleton** - all members are static, there is no instance state. This establishes clipboard access as a global service.

---

## 2. Dependencies

### Direct Dependencies

| Dependency | Location | Purpose |
|------------|----------|---------|
| `<string>` | header:2 | Text representation |
| `<functional>` | header:3 | `std::function` for callback handlers |
| `<GLFW/glfw3.h>` | cpp:2 | Default clipboard implementation |

### Dependency Analysis

**GLFW coupling** (cpp:2, cpp:27-31, cpp:42-45): The module has a hard compile-time dependency on GLFW, even though the design intent (per header:5-6) is to be "platform-agnostic." The GLFW header is unconditionally included, meaning:
- Web builds must still link or stub GLFW headers
- The abstraction is runtime-only, not compile-time

**Implicit GLFW window dependency** (cpp:27, cpp:42): Uses `glfwGetCurrentContext()` to obtain the window handle. This creates an implicit dependency on:
1. A valid OpenGL/GLFW context existing
2. The calling thread being the one that created the context

---

## 3. Mutation Points

### Global Static State

| Member | Location | Mutated By |
|--------|----------|------------|
| `s_getText` | header:26, cpp:5 | `setHandlers()`, `useDefaultHandlers()` |
| `s_setText` | header:27, cpp:6 | `setHandlers()`, `useDefaultHandlers()` |
| `s_useCustom` | header:28, cpp:7 | `setHandlers()`, `useDefaultHandlers()` |

### Authority Concerns

1. **No ownership model**: Any code can call `setHandlers()` at any time, potentially mid-operation. There is no lifecycle management or ownership assertion.

2. **Thread safety absent**: Static state is modified without synchronization (cpp:9-13, cpp:15-19). If `setHandlers()` is called while `getText()`/`setText()` is executing, behavior is undefined.

3. **External system state**: The actual clipboard is owned by the OS. The module has no visibility into external mutations (another app changing clipboard content).

---

## 4. Boundary Violations

### GLFW in Engine Layer

The inclusion of `<GLFW/glfw3.h>` directly in `clipboard.cpp` (line 2) is a **layering violation** if the engine is intended to be windowing-library-agnostic.

**Evidence of intended abstraction** (header:5-6):
```cpp
// Platform-agnostic clipboard interface
// Desktop uses GLFW, web can provide custom implementations via callbacks
```

Yet the implementation hard-codes GLFW rather than receiving it through dependency injection or a platform interface.

### Implicit Context Acquisition

The pattern at cpp:27 and cpp:42:
```cpp
GLFWwindow* window = glfwGetCurrentContext();
```

This reaches out to GLFW's global state to find "the current window." This:
- Violates explicit dependency passing
- Assumes single-window or that "current context" is meaningful
- Creates hidden coupling between clipboard operations and OpenGL context management

---

## 5. Declared-but-Unrealised Design

### Platform Abstraction is Compile-Time Incomplete

**Declared** (header:5-6): "Platform-agnostic clipboard interface"

**Reality**: The abstraction only works at runtime via callbacks. At compile time:
- GLFW is always included
- No `#ifdef` for web/other platforms
- Cannot build without GLFW headers present

**Workaround required**: Web builds would need to either:
1. Provide stub GLFW headers (brittle)
2. Modify this file with preprocessor guards (not implemented)

### Handler Validation is Asymmetric

**Declared** (header:12-13): Ability to set custom handlers

**Reality** (cpp:12):
```cpp
s_useCustom = (getText != nullptr && setText != nullptr);
```

If only one handler is provided, both are silently stored but `s_useCustom` becomes false. The stored non-null handler becomes dead code - it exists but will never be called. No error is raised.

**Missing**: Validation that would either:
- Reject partial handler sets
- Log a warning
- Allow asymmetric handlers (custom get, default set)

### Silent Failure on Missing Context

**Code** (cpp:27-32, cpp:42-45):
```cpp
GLFWwindow* window = glfwGetCurrentContext();
if (window) {
    // ... do clipboard operation
}
return ""; // or just return without action
```

When no GLFW context exists:
- `getText()` returns empty string (indistinguishable from empty clipboard)
- `setText()` silently does nothing

**No indication** that the operation failed vs. succeeded with empty data. Callers cannot distinguish:
- Clipboard is empty
- Clipboard access failed
- No window context available
- GLFW not initialized

### `hasCustomHandlers()` is Query-Only

**Declared** (header:23): `hasCustomHandlers()` - implies callers might need to know the handler state

**Usage**: This function exists but its purpose is unclear. If the abstraction works correctly, callers should not need to know which handler is in use. Its presence suggests:
1. The abstraction leaks
2. There's conditional behavior somewhere that depends on this
3. It's diagnostic-only cruft

No call sites are visible in this module to explain why external code would need this information.

---

## Summary of Architectural Concerns

| Concern | Severity | Location |
|---------|----------|----------|
| Hard GLFW dependency despite "platform-agnostic" claim | Medium | cpp:2 |
| No thread safety on global state | High | cpp:5-7, cpp:9-19 |
| Silent failure modes | Medium | cpp:27-32, cpp:42-45 |
| Implicit context acquisition | Medium | cpp:27, cpp:42 |
| Partial handler set accepted but ignored | Low | cpp:12 |
| No lifecycle/ownership model | Medium | All static design |

The module achieves its basic goal of abstracting clipboard access but does so with a leaky abstraction that undermines its stated platform-agnostic intent. The static singleton pattern prevents proper dependency injection and testing isolation.

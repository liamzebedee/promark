# promark
Promark is a professional Markdown editor.
It is visual, like Word. It is native performance, like Word. But it runs on the web. 
Its architecture is based on Figma's architecture - C++ backend with an OpenGL core, that is transpiled into a web context, where the control logic (keyboard events, mouse events) are sent to the transpiled C++ engine and the engine renders to a WebGL context.
The current version is maintained in `protos/a`. It has not been transpiled for the web yet.

```
fn test() {
    // your code here.
}
```

Wow.
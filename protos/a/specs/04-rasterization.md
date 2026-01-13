# Rasterization

## Purpose

The Rasterizer turns the PaintTree into pixels. It owns all GPU resources and is the only component that makes OpenGL calls.

## Current Problems

GL authority is split across three components: the Rasterizer controls scissor and texture creation, the BatchRenderer controls blend mode and shaders, and the GlyphAtlas sets pixel alignment and uploads textures. FreeType faces are passed through multiple layers. Image rendering bypasses batching entirely. The Engine makes direct GL calls.

The result: nobody owns the GPU. State changes happen unpredictably. Testing requires a real GL context.

## Target Model

### Single GL Authority

One component - call it RenderBackend - makes all OpenGL calls. The Rasterizer becomes a dispatcher that walks the paint tree and tells the backend what to draw. It never touches GL state directly.

The GlyphAtlas becomes an internal detail of the backend, not a separate component with its own GL calls.

The Engine never makes GL calls - it delegates everything to the rasterizer.

### Tree Traversal with Culling

The rasterizer walks the PaintTree recursively. For each PaintArtifact:
1. Check if bounds intersect the viewport - if not, skip this entire subtree
2. Apply clip if present
3. Draw the artifact's display items
4. Recurse into children
5. Restore clip if needed

This is more efficient than a flat list because entire branches can be skipped with one bounds check.

### Batching by State

The backend batches draw calls that share the same state. Consecutive solid rectangles batch together. Consecutive text draws (same texture) batch together. State changes flush the current batch.

Images currently break batching because each uses a different texture. Future optimization: texture atlases for images.

### Font Provider Hidden

FreeType doesn't appear in the rasterizer's public interface. The backend owns font resources and exposes only abstract operations: "draw this text at this position with this style." How glyphs are rasterized and cached is an internal detail.

### Resource Management

The backend owns all GPU resources: textures, shaders, buffers. It provides operations to create and destroy textures for images. The ImageLoader creates texture handles through the backend, not directly.

## What the RenderBackend Does

- **Frame lifecycle**: beginFrame, endFrame
- **Primitives**: drawRect, drawRoundedRect, drawLine
- **Text**: drawText (receives pre-shaped glyph data from layout)
- **Images**: drawImage (texture ID, source rect, dest rect, tint)
- **Clipping**: pushClip, popClip
- **Resources**: createTexture, deleteTexture

## What Gets Deleted

- Direct GL calls from Rasterizer (use backend instead)
- Direct GL calls from Engine (delegate to rasterizer)
- FreeType types from public APIs
- The standalone GlyphAtlas component (absorbed into backend)
- The file-based image decoder stubs that were never implemented

## Success Criteria

All GL calls happen in one place. FreeType is invisible outside the backend. The rasterizer walks the paint tree and dispatches to the backend. Entire subtrees can be skipped if offscreen. Testing can use a mock backend without a real GL context.

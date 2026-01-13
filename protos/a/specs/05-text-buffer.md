# Text Buffer

## Purpose

The TextModel is the single authority for document content. It answers the question: "What is the current text?"

## Current Problems

Three copies of the document exist: a raw char array in the Engine (the actual authority), a TextBuffer in the Engine (bypassed), and another TextBuffer in the MarkdownRenderer (a copy). Every edit requires three operations: memmove on the raw buffer, setText to sync the TextBuffer, and a copy to the renderer.

The shell maintains parallel dirty tracking by comparing strings every frame, ignoring the Engine's isDirty method.

The TextBuffer has insertText and deleteText methods that are implemented but never called.

The result: 18+ synchronization sites, O(n) copying per keystroke, and two dirty-tracking systems that can diverge.

## Target Model

### Single Authority

One component - the TextModel in the Engine - owns the document content. It's the only thing that can modify text.

The raw char array is gone. The bypassed TextBuffer is gone. There's one source of truth.

### Immutable Snapshots

When downstream components need the text (for parsing), they receive an immutable snapshot. The parser cannot modify the text it receives. The snapshot is created once per frame when content has changed, not once per keystroke.

This replaces the current pattern of copying on every edit with a single copy when needed.

### One Dirty Flag

The Engine tracks whether content has changed since last save. The shell queries this flag - it doesn't maintain its own shadow copy and string comparison.

When the shell saves, it tells the Engine to mark clean. One source of truth for dirty state.

### Operation-Based Undo

Text mutations are recorded as operations (insert at position, delete at position). Undo reverses the last operation. Redo re-applies it.

Each undo entry stores:
- The text operation (what changed)
- Caret position before the operation
- Scroll position before the operation

Undo restores all three: content, caret, and scroll. The user sees exactly what they saw before the undone action.

This replaces the current single-level "save entire state" approach with a proper undo stack.

## What the TextModel Does

- **Mutations**: insert, delete, replace at position
- **Bulk operations**: setText, clear
- **Queries**: length, charAt, substring
- **Snapshots**: produce immutable view for downstream
- **Versioning**: increment version on change for cache invalidation

## What the Snapshot Provides

- **Read-only access**: data pointer, length
- **Version number**: for downstream caching
- **Ownership**: snapshot owns its data (doesn't reference mutable source)

## What Gets Deleted

- The inputBuffer char array in Engine
- The TextBuffer's never-used insertText/deleteText methods
- The parallel diskContent string in the shell
- The O(n) string comparison every frame
- The dual-write synchronization pattern

## Success Criteria

One component owns text content. Downstream receives immutable snapshots. One dirty flag exists, in the Engine. No per-keystroke O(n) copies. Undo/redo works with operation stack.

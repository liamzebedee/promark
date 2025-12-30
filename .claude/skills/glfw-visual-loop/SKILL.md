---
name: glfw-visual-loop
description: >
  Visual feedback loop for native macOS C++/GLFW apps. Use this skill when working on a 
  GLFW-based C++ application that renders to a window and you need to verify visual changes 
  actually worked. Trigger when making UI or rendering changes and need to confirm they look 
  correct, when user asks to verify something visual like "check if the font renders correctly", 
  when debugging visual issues, or any time you need to "see" what the app currently looks like. 
  This skill builds with make, launches the app, captures a screenshot, and returns it for 
  visual inspection.
---

# GLFW Visual Loop

Visual feedback loop for C++/GLFW apps on macOS.

## Workflow

1. Make code changes
2. Run the capture script to build, launch, and screenshot
3. Inspect the returned image to verify changes
4. Iterate as needed

## Usage

```bash
bash scripts/capture_glfw_app.sh <project_dir> [output_image.png]
```

Arguments:
- `project_dir`: Directory containing the Makefile (required)
- `output_image.png`: Where to save screenshot (optional, defaults to /tmp/glfw_capture_<timestamp>.png)

## Script behavior

1. Runs `make` in project directory
2. Finds executable by parsing Makefile for TARGET/EXECUTABLE/BIN/APP variables, or falls back to finding most recently modified executable
3. Launches app in background
4. Waits 5 seconds or less for window to appear
5. Captures window screenshot using `screencapture -l <windowID>`
6. Kills the app
7. Outputs the screenshot path

## After capture

View the screenshot image to verify visual correctness. Compare against expected appearance and report findings to user.

## Error handling

- If `make` fails: Compilation errors are printed—fix code and retry
- If executable not found: Check Makefile defines a target or produces an executable
- If window not detected: Falls back to full screen capture

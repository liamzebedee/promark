#!/bin/bash
# Builds, runs, screenshots a GLFW app, then cleans up.
# Usage: capture_glfw_app.sh <project_dir> [output_image_path]

set -e

PROJECT_DIR="${1:-.}"
OUTPUT_PATH="${2:-/tmp/glfw_capture_$(date +%s).png}"

cd "$PROJECT_DIR"

# Build
echo "Building..."
make 2>&1

# Find executable: check Makefile for target, or find recent executable
find_executable() {
    # Try parsing Makefile for common target patterns
    if [[ -f Makefile ]]; then
        # Look for "TARGET = xxx" or "EXECUTABLE = xxx" patterns
        target=$(grep -E '^\s*(TARGET|EXECUTABLE|BIN|APP)\s*[:=]' Makefile 2>/dev/null | head -1 | sed 's/.*[:=]\s*//' | tr -d ' ')
        if [[ -n "$target" && -x "$target" ]]; then
            echo "$target"
            return
        fi
        # Look for first target that produces an executable
        target=$(grep -E '^[a-zA-Z0-9_-]+:' Makefile | head -1 | sed 's/:.*//')
        if [[ -n "$target" && -x "$target" ]]; then
            echo "$target"
            return
        fi
    fi
    # Fallback: find most recently modified executable in current dir
    find . -maxdepth 2 -type f -perm +111 ! -name "*.sh" ! -name "Makefile" -newer Makefile 2>/dev/null | head -1
}

EXECUTABLE=$(find_executable)
if [[ -z "$EXECUTABLE" || ! -x "$EXECUTABLE" ]]; then
    echo "Error: Could not find executable. Check Makefile or build output." >&2
    exit 1
fi

echo "Found executable: $EXECUTABLE"

# Launch app in background
"$EXECUTABLE" &
APP_PID=$!
echo "Launched with PID: $APP_PID"

# Wait for window to appear (poll for up to 5 seconds)
echo "Waiting for window..."
WINDOW_ID=""
for i in {1..50}; do
    sleep 0.1
    # Get window ID using osascript - find window by PID
    WINDOW_ID=$(osascript -e "
        tell application \"System Events\"
            set targetProcess to first process whose unix id is $APP_PID
            if exists (window 1 of targetProcess) then
                return id of window 1 of targetProcess
            end if
        end tell
    " 2>/dev/null || true)
    
    if [[ -n "$WINDOW_ID" ]]; then
        break
    fi
done

# Alternative: try getting window by looking for GLFW windows
if [[ -z "$WINDOW_ID" ]]; then
    # Use CGWindowListCopyWindowInfo approach via screencapture -l with window search
    WINDOW_ID=$(osascript -e "
        tell application \"System Events\"
            repeat with proc in (processes whose unix id is $APP_PID)
                if exists window 1 of proc then
                    return id of window 1 of proc
                end if
            end repeat
        end tell
    " 2>/dev/null || true)
fi

if [[ -z "$WINDOW_ID" ]]; then
    echo "Warning: Could not get window ID, capturing full screen" >&2
    sleep 0.5
    screencapture -x "$OUTPUT_PATH"
else
    echo "Capturing window ID: $WINDOW_ID"
    screencapture -l "$WINDOW_ID" -x "$OUTPUT_PATH"
fi

# Cleanup
kill $APP_PID 2>/dev/null || true
wait $APP_PID 2>/dev/null || true

echo "Screenshot saved: $OUTPUT_PATH"

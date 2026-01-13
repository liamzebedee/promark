#!/bin/bash
# Claude Code JSON output pretty printer library
# Source this file and use: run_claude_formatted <args...>
# Or pipe to: format_claude_json

# Colors
CLAUDE_CYAN='\033[0;36m'
CLAUDE_GREEN='\033[0;32m'
CLAUDE_YELLOW='\033[0;33m'
CLAUDE_GRAY='\033[0;90m'
CLAUDE_BOLD='\033[1m'
CLAUDE_RESET='\033[0m'

# Format a single line of Claude stream-json output
format_claude_line() {
  local line="$1"

  # Skip empty lines
  [[ -z "$line" ]] && return

  # Check if it's valid JSON
  if ! echo "$line" | jq -e . >/dev/null 2>&1; then
    # Not JSON, print as-is (loop status messages, etc.)
    echo "$line"
    return
  fi

  local msg_type=$(echo "$line" | jq -r '.type // empty')

  case "$msg_type" in
    assistant)
      # Check for text content
      local text=$(echo "$line" | jq -r '.message.content[]? | select(.type == "text") | .text // empty' 2>/dev/null)
      if [[ -n "$text" ]]; then
        echo -e "\n${CLAUDE_GREEN}${text}${CLAUDE_RESET}\n"
      fi

      # Check for tool use
      local tool_name=$(echo "$line" | jq -r '.message.content[]? | select(.type == "tool_use") | .name // empty' 2>/dev/null)
      if [[ -n "$tool_name" ]]; then
        if [[ "$tool_name" == "TodoWrite" ]]; then
          # Format TodoWrite specially with status icons
          echo -e "${CLAUDE_CYAN}▶ TodoWrite${CLAUDE_RESET}"
          echo "$line" | jq -r --arg g "$CLAUDE_GREEN" --arg y "$CLAUDE_YELLOW" --arg gr "$CLAUDE_GRAY" --arg r "$CLAUDE_RESET" \
            '.message.content[]? | select(.type == "tool_use") | .input.todos[]? | "  \(if .status == "completed" then $g + "✓" elif .status == "in_progress" then $y + "→" else $gr + "○" end) \(.content)\($r)"' 2>/dev/null | while IFS= read -r todo; do echo -e "$todo"; done
        else
          local tool_input=$(echo "$line" | jq -r '.message.content[]? | select(.type == "tool_use") | .input | to_entries | map("\(.key): \(.value | if type == "string" then (if (. | length) > 80 then .[0:80] + "..." else . end) else . end)") | join(", ")' 2>/dev/null)
          echo -e "${CLAUDE_CYAN}▶ ${tool_name}${CLAUDE_RESET} ${CLAUDE_GRAY}${tool_input}${CLAUDE_RESET}"
        fi
      fi
      ;;

    user)
      # Tool results - show abbreviated
      local tool_id=$(echo "$line" | jq -r '.message.content[]? | select(.type == "tool_result") | .tool_use_id // empty' 2>/dev/null | head -1)
      if [[ -n "$tool_id" ]]; then
        local result_preview=$(echo "$line" | jq -r '.message.content[]? | select(.type == "tool_result") | .content // empty' 2>/dev/null | head -3 | tr '\n' ' ' | cut -c1-100)
        if [[ -n "$result_preview" ]]; then
          echo -e "${CLAUDE_GRAY}  ← ${result_preview}...${CLAUDE_RESET}"
        fi
      fi
      ;;

    result)
      # Final result
      local result_text=$(echo "$line" | jq -r '.result // empty' 2>/dev/null)
      if [[ -n "$result_text" ]]; then
        echo -e "\n${CLAUDE_BOLD}${CLAUDE_GREEN}═══ RESULT ═══${CLAUDE_RESET}"
        echo -e "${CLAUDE_GREEN}${result_text}${CLAUDE_RESET}"
      fi
      ;;

    error)
      local error_msg=$(echo "$line" | jq -r '.error.message // .error // empty' 2>/dev/null)
      echo -e "${CLAUDE_YELLOW}ERROR: ${error_msg}${CLAUDE_RESET}"
      ;;
  esac
}

# Process stdin and format each line
format_claude_json() {
  while IFS= read -r line; do
    format_claude_line "$line"
  done
}

# Run claude with formatting applied to output
# Usage: run_claude_formatted [claude args...]
run_claude_formatted() {
  claude "$@" 2>&1 | format_claude_json
}

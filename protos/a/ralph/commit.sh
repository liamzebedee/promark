#!/bin/bash
# Usage: ./commit.sh [message_hint]
# Examples:
#   ./commit.sh                    # Auto-generate commit message
#   ./commit.sh "fix auth bug"     # Use hint for commit message context

MESSAGE_HINT="${1:-}"
CURRENT_BRANCH=$(git branch --show-current)

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Ralph Commit"
echo "Branch: $CURRENT_BRANCH"
[ -n "$MESSAGE_HINT" ] && echo "Hint:   $MESSAGE_HINT"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Build the prompt
PROMPT="Create a git commit for the current changes.

Instructions:
1. Run git status to see all untracked files (never use -uall flag)
2. Run git diff to see both staged and unstaged changes
3. Run git log --oneline -10 to see recent commit message style
4. Analyze all changes and draft a commit message that:
   - Summarizes the nature of changes (new feature, enhancement, bug fix, refactor, etc.)
   - Focuses on the 'why' rather than the 'what'
   - Is concise (1-2 sentences)
   - Follows the repository's existing commit style
5. Stage relevant files with git add (exclude secrets like .env, credentials.json)
6. Create the commit using a HEREDOC format:

git commit -m \"\$(cat <<'EOF'
Commit message here.

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
EOF
)\"

7. Run git status after commit to verify success

IMPORTANT:
- Do NOT push to remote
- Do NOT use git commands with -i flag (interactive mode not supported)
- Do NOT commit files that likely contain secrets
- If there are no changes to commit, report that and exit"

if [ -n "$MESSAGE_HINT" ]; then
    PROMPT="$PROMPT

User's hint for commit message context: $MESSAGE_HINT"
fi

# Run Claude to create the commit
echo "$PROMPT" | claude -p \
    --dangerously-skip-permissions \
    --output-format=stream-json \
    --model sonnet \
    --verbose

echo -e "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Commit complete"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

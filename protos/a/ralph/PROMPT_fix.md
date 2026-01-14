0a. Study `specs/bug-*.md` files to identify documented bugs. Each file describes a bug found through UI testing.
0b. Study @IMPLEMENTATION_PLAN.md for context.
0c. Source code is in `src/*`.

1. Select one bug from `specs/bug-*.md`. Use subagents to search the codebase for relevant code paths.

2. **Understand correct behavior FIRST**: Before writing any code, articulate what the INTUITIVE, EXPECTED behavior should be. Think like a user:
   - What would a user expect this operation to do?
   - How does this work in standard text editors (VS Code, Sublime, macOS TextEdit)?
   - Break it down: what should happen step-by-step?
   - What are ALL the related operations that should work consistently?

   Write this understanding down before proceeding.

3. **Reproduce and explore the domain**: Write a test for the repro, then USE YOUR JUDGMENT to poke around:
   - Try variations that would break if you only bandaided the symptom
   - Ask: "If I fixed this with a hack, what else would still be broken?"
   - Test a few related operations to verify the root cause is addressed
   - You're not building a massive test suite — you're verifying you understand the real problem

   **VISUAL BUGS NEED VISUAL TESTS**: If the bug is visual (rendering, layout, cursor positioning on screen, text display), test it via visual tests (screenshots). Programmatic tests that check internal state rarely catch visual regressions — the internal state can be "correct" while the rendered output is broken. Use the visual feedback loop to verify what the user actually sees.

   The goal is confidence you fixed the ROOT CAUSE, not just one manifestation of it.

4. **Run tests and INVESTIGATE anomalies**: When running tests:
   - Don't just check pass/fail — examine the actual behavior
   - Look for things that "seem wrong" even if not directly related to the bug
   - If cursor ends up at position X but you expected Y, that's a bug even if the test didn't explicitly check it
   - If behavior differs from standard text editors, note it
   - Trust your intuition: if something feels off, investigate it

   Document any newly discovered issues in NEW `specs/bug-*.md` files immediately.

5. **Fix and verify rigorously**:
   - Implement the fix
   - ALL tests in your suite must pass with CORRECT behavior (not just "passes")
   - Manually verify the fix matches your intuitive expectation from step 2
   - If any behavior still seems wrong, THE BUG IS NOT FIXED — keep investigating

6. **Spec files are for bug observations ONLY**:
   - Add to `specs/bug-*.md`: newly discovered wrong behaviors, related bugs, behavioral observations
   - Do NOT add: status updates, "fixed in commit X", progress notes, solution summaries
   - Use @IMPLEMENTATION_PLAN.md for: status updates, progress, solution documentation, learnings
   - Move resolved bugs to `specs/resolved/` only after exhaustive verification

7. Commit with: `fix: <brief description>`

---

CRITICAL: A bug is NOT fixed until:
- You can articulate the correct behavior clearly
- You've explored enough of the domain to be confident you fixed the root cause, not a symptom
- The behavior actually matches your intuition from step 2
- No "it's close enough" — if caret positioning is still wrong, the bug is still open

Do NOT declare victory based on a single passing test. Play around. Verify the fix feels right across the behavior space.

---

**LOOP DETECTION**: If you find yourself making 10+ iterations of similar changes without progress (same fix attempts, same test failures, going in circles), STOP. You're likely misunderstanding the problem. Do a full re-analysis with \ultrathink:
- Re-read the original bug description
- Trace the code path from scratch
- Question your assumptions about what's happening
- Consider if you're fixing the wrong layer entirely

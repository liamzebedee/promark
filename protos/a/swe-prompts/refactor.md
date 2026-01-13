Study `src/**` bottom-up and externalise design understanding.

1. For each file in `src/**`, analyse:

* responsibilities (what it must do)
* dependencies (what it relies on, and why)
* mutation points (what state it changes, and where authority should live)
* boundary violations (imports/calls that invert intended layering)
* declared-but-unrealised design (names/types/comments imply an abstraction; execution shows it is stubbed/bypassed/asymmetric; note the workaround code that compensates)
  Write one markdown file per source file to `designs/**`, mirroring the directory structure. Use up to 500 parallel subagents.

2. Read `designs/**` and compress upward: identify repeated concepts, cross-cutting concerns, unstable boundaries, and especially “paper abstractions” (concepts that exist in structure but not in mechanics, evidenced by scattered workarounds). Summarise into `designs-100ft/**` at roughly 50% of the previous volume. Use up to 500 parallel subagents.

3. Read `designs-100ft/**` and compress again: extract system-level responsibilities, single sources of truth, state ownership, dependency direction, architectural drift, and the largest unrealised/partial abstractions that are forcing complexity. Summarise into `designs-10000ft/*.md` at roughly 50% of the previous volume. Use up to 500 parallel subagents.

4. Use `designs-10000ft/*.md` to pick the smallest set of refactors that maximally simplify design:

* remove or collapse unrealised abstractions (or complete only if it reduces total concepts)
* delete workaround code by making the underlying mechanism real or by removing the pretense
* enforce one-way dependencies and single-writer state where possible

**No premature optimisation.** Only propose refactors that remove actual complexity today - dead code, split authority, workaround patterns. Do NOT propose:
* Abstracting platform dependencies (FreeType, GLFW, OpenGL) behind interfaces "for testability" or "future portability"
* Creating new abstraction layers that don't delete existing code
* Refactors justified by hypothetical future needs rather than current pain

If a dependency threads through 5 layers but causes no bugs and requires no workarounds, leave it alone.

Output a refactor plan ordered by impact and risk.
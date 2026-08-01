# Task Plan

## Goal
Add an operator-panel control that changes the T6 balance target `R` before starting the task, while preserving existing task behavior and parameter limits.

## Phases
1. [completed] Inspect T6 target handling, task selection, display, and button mappings.
2. [completed] Implement a bounded T6-only `R` adjustment using existing interaction patterns.
3. [completed] Build and verify the affected firmware behavior.

## Errors Encountered
| Error | Attempt | Resolution |
|---|---:|---|
| PowerShell `rg` rejected Unix-style file globs in positional paths. | 1 | Use explicit file paths or `--glob` patterns. |

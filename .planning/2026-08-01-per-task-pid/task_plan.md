# Task Plan

## Goal
Give T3, T4, T5, and T6 independent explicitly assigned control parameters, give T3 separate +5 cm and -5 cm parameter sets, and make T3 transition its reference from +5 cm to -5 cm after reaching the first target.

## Requirements
- Do not generate task configurations with loops.
- Keep every requested control field visibly assigned for every task/stage.
- T3 starts at +5 cm, then switches to -5 cm with its second parameter set.
- Preserve runtime tuning and safety behavior where compatible.

## Phases
1. [completed] Inspect the current configuration ownership, T3 phase transition, and runtime tuning path.
2. [completed] Design independent task/stage parameter structures and explicit application assignments.
3. [completed] Implement task-specific selection and repair the T3 +5 cm to -5 cm transition.
4. [completed] Build, inspect state-transition behavior, and document validation.
5. [completed] Make the existing runtime tuning command select exactly one independent profile.

## Errors Encountered
| Error | Attempt | Resolution |
| --- | --- | --- |
| Combined source check exited 1 | 1 | `rg` found zero obsolete references, which is success for this check; run build and whitespace checks separately. |
| Final parallel check exited 1 | 1 | The compile-command Ozone macro search intentionally had zero matches; rerun with an explicit zero-match success branch. |
| Cross-file runtime-selector patch failed to write the `.cpp` file | 1 | The header portion applied; split the implementation edit into smaller single-file patches. |

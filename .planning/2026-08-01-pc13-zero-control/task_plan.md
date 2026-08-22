# Task Plan: PC13 Zero Control

## Goal
Add a PC13 key available only in the VIEW screen: a long press sets the motor's current position as the single-turn homing zero, while a short click triggers single-turn nearest-origin homing using the ZDT_X42S V1.0.5 protocol. Show the stepper motor control target angle on LCD row five.

## Phases
- [completed] Phase 1: Inspect the complete manual sections, PC13 CubeMX configuration, VIEW state handling, key input patterns, and motor protocol implementation
- [completed] Phase 2: Define debounce, short/long press semantics, VIEW-only gating, and exact motor frames
- [completed] Phase 3: Implement GPIO, motor protocol, and UI/controller integration with focused tests where practical
- [completed] Phase 4: Build the firmware and resolve compile/link issues
- [completed] Phase 5: Review the diff and document hardware-only verification needs

## Constraints
- Preserve unrelated user changes.
- Reuse the firmware's existing key/input, VIEW-state, UART, and motor APIs.
- Do not trigger either zero command outside the VIEW screen.
- A long press emits one set-zero command per press and does not emit a short-click command on release.
- Do not claim physical motor verification without observing the target hardware.

## Errors Encountered
| Error | Attempt | Resolution |
|---|---:|---|
| Parallel inspections containing an `rg` no-match result were aggregated as exit code 1 | 1 | Used settled independent commands so genuine results and no-match cases remain distinguishable. |
| Existing `.planning/.active_plan` targeted an unrelated Ozone task | 1 | Created this isolated plan and switched the pointer. |

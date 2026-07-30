# Progress: Ozone FreeRTOS Pause Diagnosis

## 2026-07-30
- Read `computer-use` and `planning-with-files` instructions.
- Preserved the unrelated root planning files and created an isolated diagnosis plan.
- Read required Computer Use guidance and confirmation policy.
- Located `build/Debug/26diansai.elf` and identified the code as `xTaskResumeAll()`.
- Inspected Ozone: it is a disconnected `*New Project`; source selection was mistaken for a live program counter.
- Inspected File/Debug menus and chose to reuse the existing loaded project state instead of replacing it with the new-project wizard.
- Attached to the running STM32 without resetting/programming it.
- Captured the real halted PC in `prvTasksExitError()` and narrowed the issue to a task function returning.
- Verified the FreeRTOS Cortex-M stack initialization and inspected application task entry paths; identified `ball_motor` as the leading suspect.
- Opened Ozone Global Data view to inspect `pxCurrentTCB`.

## Test Results
| Check | Result |
|---|---|
| Locate current ELF | `build/Debug/26diansai.elf` exists |
| Identify quoted FreeRTOS function | `xTaskResumeAll()` at `tasks.c:2278-2298` |
| Initial Ozone target state | Disconnected; empty call stack |
| Attach and halt running target | Connected at 4 MHz; halt succeeded |
| Live PC | `prvTasksExitError()` terminal loop, not `xTaskResumeAll()` |

## Error Log
| Error | Attempt | Resolution |
|---|---|---|
| One parallel static-search command returned exit code 1 and canceled grouped output | 1 | Re-ran checks with per-command tolerance and found the ELF and source location |
| A second grouped `rg` scan was canceled by a no-match exit code | 1 | Switched the scan to tolerant commands and obtained the relevant task-entry results |

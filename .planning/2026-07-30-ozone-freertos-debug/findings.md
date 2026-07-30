# Findings: Ozone FreeRTOS Pause Diagnosis

## User Observation
- Firmware appears to stop in the FreeRTOS path around `xYieldPending`, `taskYIELD_IF_USING_PREEMPTION()`, and `taskEXIT_CRITICAL()`.
- Requested debugger: `C:\ProgramData\Microsoft\Windows\Start Menu\Programs\SEGGER - Ozone V3.40g\Ozone V3.40g.lnk`.

## Static Evidence
- Current debug image exists at `build/Debug/26diansai.elf` (2,475,152 bytes, built 2026-07-30 13:06:37).
- The quoted block is `xTaskResumeAll()` in `Middlewares/Third_Party/FreeRTOS/Source/tasks.c:2278-2298`.
- `configUSE_PREEMPTION` is `1`; `configASSERT` disables interrupts and loops forever on a failed assertion.
- `xTaskResumeAll()` is a normal scheduler-unlock path. The selected source lines alone do not prove execution is stuck there.
- `port.c:198-200` writes the task entry to the initial PC and `prvTaskExitError` to the initial LR. `port.c:221-238` explicitly states a task must not return and then loops forever.
- `StartDefaultTask()` calls `app_main()`, which creates `maxican_rx`, `ball_motor`, and `terminal`, then remains in an infinite sleep loop.
- `maxican_rx` normally enters `BallReceiver::Run()` and its infinite loop.
- `BallTrackingThread()` returns immediately if its parameters are invalid, `motor->Enable(true)` fails, or `SetCurrentPositionAsZero()` fails. With the current serial motor startup path, `ball_motor` is the leading suspect.

## Ozone Evidence
- Initial Ozone window title is `*New Project`.
- Status bar shows `Disconnected` and `Ready`.
- Call Stack pane is empty.
- Source view is open at `tasks.c` around lines 2278-2298 with the block text selected; this is a source selection, not a current-PC marker.
- Disassembly pane contains the corresponding compiled code, confirming symbols/source were indexed previously, but no live target state is available while disconnected.
- The existing unsaved `New Project` already has the ELF's symbols/source loaded. `Debug > Start Debug Session (F5)` is enabled; live controls are disabled until connection.
- A new-project wizard is available but was not used, avoiding replacement of the user's unsaved current Ozone configuration.
- Used `Start Debug Session > Attach & Halt Program`, preserving the currently running flash image.
- Live connection succeeded (`Connected @ 4 MHz`).
- Current PC is in `prvTasksExitError()` in the Cortex-M4 FreeRTOS `port.c`; source highlights the `configASSERT`/terminal-loop area and disassembly shows a branch to itself at `0x08006FF0`.
- Call Stack lists `prvTasksExitError` as frame 0 and `xTaskResumeAll` below it. The latter is not the current PC and may be residual stack/unwind context after the task returned.

## Diagnosis
- Confirmed root-cause class: a FreeRTOS task entry function returned. The Cortex-M port initializes every task's LR to `prvTaskExitError`; returning from the task reaches this deliberate terminal loop. `ball_motor` is the leading suspect because its entry returns on motor initialization/communication errors; TCB-name confirmation is in progress.

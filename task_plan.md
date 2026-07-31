# Task Plan: Vehicle Balance-Ball Firmware Integration

## Goal
Integrate the existing vision result stream, X42S stepper motor driver, LCD, two PA4/PA5 task-selection buttons, LibXR topic messaging, and resource-light command-line diagnostics into a safe STM32 balance-ball control application that follows the H-problem rules and the repository architecture.

## Current Phase
Phase 16 - Diagnose and correct X42S motion scaling

## Active Subtask
Read the connected motor configuration without motion, prove the command/feedback units and pulse scaling, apply only an evidence-backed correction, then repeat the bounded Ozone `+2` degree and return-to-zero verification.

Current checkpoint: user reduced the requested motion to left 10 degrees then right 10 degrees. Ozone is targetable and the last verified CPU state is halted. A read-only configuration query is staged as operation 6, unlock `0x58423432`, request/completed `1/0`; foreground input interrupted before Continue. No motion parameters or command are staged. Complete this query, validate scaling, zero, then execute `-10 -> 0` at 1 RPM/acceleration 1 with per-step readback.

## Phases

### Phase 1: Read requirements and inspect architecture
- [x] Read `xrobot-agent-context.md` completely.
- [x] Extract relevant rules, task definitions, dimensions, timing, and scoring constraints from the H-problem PDF.
- [x] Inspect project structure, build system, current application entry points, and local repository instructions.
- [x] Inspect `bujin_motor`, `maxican`, `LCD`, and all current integration call sites.
- [x] Preserve and incorporate the ZDT X42S TTL protocol extraction for `bujin_motor`.
- [x] Study BMI088 topic publication/subscription and command-line print activation patterns.
- **Status:** complete

### Phase 2: Define interfaces and control architecture
- [x] Determine the vision transport/protocol already configured in the project.
- [x] Define a bounded, validated vision-result message and stale-data behavior.
- [x] Map PA4/PA5 input events to task selection and start/stop behavior.
- [x] Define control states, safety limits, task trajectories, motor units, and LCD status model.
- [x] Record assumptions that require hardware tuning.
- **Status:** complete

### Phase 3: Implement data and diagnostics layer
- [x] Implement or integrate vision frame reception and validation.
- [x] Publish vision/control state through LibXR topics.
- [x] Add command-line diagnostics that print only when explicitly enabled.
- **Status:** complete

### Phase 4: Implement operator interface
- [x] Configure PA4/PA5 as debounced inputs without disturbing board-generated setup.
- [x] Implement task selection and run-state switching.
- [x] Display selected task, run state, vision validity, ball position, and faults on LCD.
- **Status:** complete

### Phase 5: Implement balance and task control
- [x] Implement deterministic periodic feedback control using position and velocity.
- [x] Convert controller output to bounded stepper commands with safe enable/stop behavior.
- [x] Implement the contest task modes supported by the mechanical/vision information available.
- **Status:** complete

### Phase 6: Integrate and verify
- [x] Build the actual firmware target.
- [x] Run available static checks and inspect protocol/control edge cases.
- [x] Fix integration errors while preserving unrelated user changes.
- [x] Verify the `bujin_motor` TTL driver builds.
- **Status:** complete

### Phase 7: Deliver
- [x] Document button behavior, CLI commands, topic names, LCD states, and tuning constants.
- [x] Document `bujin_motor` TTL usage and protocol notes in `READMA.md`.
- [x] Report build results and hardware-only calibration steps.
- **Status:** complete

### Phase 8: Move terminal to USART2
- [x] Route LibXR STDIO input/output from USB CDC to USART2.
- [x] Update terminal connection instructions.
- [x] Rebuild the firmware target.
- **Status:** complete

### Phase 9: Correct terminal diagnostic argument parsing
- [x] Reproduce why `ball show 10000 100` fails validation despite valid positive arguments.
- [x] Fix parsing while retaining the documented command interface.
- [x] Rebuild the Debug target and document the result.
- **Status:** complete

### Phase 10: Diagnose missing vision UART frames
- [ ] Trace the vision sender format through the STM32 USART1 receiver.
- [ ] Verify serial-instance, pin, DMA, interrupt, and baud-rate configuration.
- [ ] Report the evidence-backed failure point and hardware checks.
- **Status:** in_progress

### Phase 11: Migrate the MaixCAM UART receiver to the revised binary protocol
- [x] Replace legacy ASCII frame parsing with the documented 13-byte binary decoder.
- [x] Preserve downstream measurements by converting fixed-point little-endian fields to the existing public units.
- [x] Verify valid, invalid, checksum-failure, and stream-resynchronization behavior.
- **Status:** complete

### Phase 12: Improve LCD operator-panel readability
- [x] Inspect the current display geometry, font metrics, and panel layout.
- [x] Increase the text size and move the displayed status content rightward without clipping it.
- [x] Rebuild the firmware and check the resulting source diff.
- **Status:** complete

### Phase 13: Display stepper motor position telemetry
- [x] Publish the X42S encoder angle and latest command target through `BalanceStatus`.
- [x] Read the encoder from the controller task at a bounded rate so the LCD task never accesses USART3.
- [x] Add a sixth LCD row while preserving the enlarged font and rebuild the Debug firmware.
- **Status:** complete

### Phase 14: Add a default LCD viewing mode
- [x] Add a no-control monitor task before T3 through T6 and select it at boot.
- [x] Refresh vision and motor telemetry in monitor mode without issuing motor control commands.
- [x] Update LCD task naming/cycling and rebuild the Debug firmware.
- **Status:** complete

### Phase 15: Hardware test X42S stepper over USART3 with Ozone
- [x] Inspect the current build/debug artifacts, Ozone project configuration, USART3/X42S settings, and available serial ports.
- [x] Build the current Debug firmware and use Ozone V3.40g to program/start it on the target.
- [x] Verify that the X42S returns valid position telemetry through USART3 before commanding motion.
- [x] Exercise one low-speed `+2` degree move and record the failed arrival result precisely; smooth arrival was not demonstrated because the request timed out at `+0.505371094` degrees.
- [x] Leave the motor stopped at the documented final safe position and report the pulse-scale/hardware-configuration limitation without issuing a return move after timeout.
- **Status:** complete (hardware verification concluded safely; smooth-position requirement failed)

### Phase 16: Diagnose and correct X42S motion scaling
- [x] Inspect the implemented protocol, motor configuration, ID, motion units, feedback units, and any mechanical transmission ratio without moving the motor.
- [x] Add or use a read-only USART3 configuration query for actual firmware/microstep/pulse settings when the documented response can be decoded safely.
- [ ] Apply the smallest evidence-backed configuration or conversion correction and add focused tests.
- [x] Build the read-only diagnostic Debug firmware and inspect the resulting diff before touching the target.
- [ ] Reload the corrected firmware in the existing Ozone session, revalidate `Monitor/Ready`, then zero and execute `+2` degrees followed by return to `0` at 1 RPM/acceleration 1 with readback after each step.
- [x] Stop immediately and preserve the last safe state on any fault, timeout, communication failure, or direction anomaly.
- **Status:** in_progress

### Phase 17: Add the VIEW bidirectional motor self-test
- [x] Start the self-test only after PA5 is pressed in `VIEW`; do not issue motor commands at boot.
- [x] Enable and zero the motor, move `+10` degrees, then `-10` degrees at conservative speed, using encoder feedback and bounded per-leg timeouts.
- [x] Stop on completion, communication failure, or timeout; update the LCD status and rebuild the Debug firmware.
- **Status:** complete

### Phase 18: Bind PA4 to VIEW RUN reverse motion
- [x] Keep PA4 as task selection in `VIEW READY`.
- [x] Treat PA4 during `VIEW RUN` as a reverse request to `-10°` at 30 RPM / acceleration 20.
- [x] Rebuild and verify the button/state behavior.
- **Status:** complete

## Decisions Made
| Decision | Rationale |
|---|---|
| Preserve existing X42S protocol research in `findings.md` | The balance controller depends on the driver and the prior extraction remains relevant. |
| Treat loss/staleness of vision data as a stop condition | A moving actuator must not continue from stale coordinates. |
| Keep debug printing opt-in | Matches the requested BMI088-style resource-light command behavior. |
| PA4 cycles the selected task; PA5 starts the selected task or aborts an active task | The available two GPIOs must support both task selection and the contest-required start action. |
| Implement vehicle-motion tasks as bounded balance modes | This MCU owns the ball/beam actuator and vision receiver; vehicle line following remains outside the stated module scope. |
| Use English LCD strings | The bundled LCD font is ASCII-only; this avoids unreadable UTF-8 glyphs on the contest display. |
| Bound terminal diagnostics to 10 seconds and 20..1000 ms | Keeps the BMI088-style opt-in diagnostic useful without monopolizing the terminal task. |

## Errors Encountered
| VIEW self-test poll throttling initially attempted to invoke a lock guard's destructor explicitly | 1 | Replaced it with a timeout flag evaluated under the lock, then called the stop path only after the lock scope ends. |
| `F6` was sent once after Ozone reconnected, but the immediate refreshed status still showed CPU running | 1 | Do not repeat `F6`; keep all mailbox writes blocked and use the installed Ozone user manual to identify a different deterministic Halt control. |
| First Phase 16 Ozone state capture displayed `Target Connection Lost: Failed to read target status. Abort Debug Session?` | 1 | Stop all target interaction immediately. Do not dismiss the modal, reconnect, write Watch, query USART3, or infer CPU state; preserve the unresolved dialog and report the hardware information needed to continue. |
| First position-decoder patch used mojibake console text as context and did not match the UTF-8 source | 1 | The patch was rejected atomically. Re-read with explicit UTF-8 and split the edit into code-only context blocks. |
| PDF text search did not find the Emm `FD` position-command table although prior extraction confirms it exists | 1 | Treat the PDF table text layer as unreliable for combined search; locate the chapter from the table of contents/page neighborhood and extract those pages directly. |
| Recursive PDF scan under the full user profile ran for about 50 seconds and ended with exit code 1 after returning partial matches | 1 | Do not repeat the broad scan; use the identified July 2026 WeChat file directory directly. |
| Ozone minimized while opening the next Watch context menu; recovery detected external user input | 1 | Stop UI input, re-enumerate read-only, and resume only after a fresh unique-window state can be captured without interference. |
| `config_parameter_count` text was not visibly confirmed after the first inline typing call | 1 | Do not commit an uncertain expression; select all in the active editor, retype, and inspect before pressing Return. |
| `Return` did not activate the selected blank Watch input row | 1 | Use standard table edit key `F2`, which immediately opened the inline editor. |
| `Alt_L+KP_Add` did not open Ozone Watch Add despite the documented `Alt+Plus` shortcut | 1 | Stop guessing key aliases; use the documented Watch context-menu `Add` path from a fresh screenshot. |
| Ozone manual extraction hit a GBK `UnicodeEncodeError` while printing PDF text | 1 | Retry the same read with `PYTHONIOENCODING=utf-8`; no UI or target action occurred. |
| Error | Attempt | Resolution |
|---|---|---|
| Python received the Chinese PDF filename with replacement characters | 1 | Resolve the real file path through PowerShell enumeration, then pass it as an argument. |
| Baseline build fails because expanded `zdt_x42s.hpp` does not match legacy `zdt_x42s.cpp` | 1 | Complete the protocol implementation/compatibility surface before integrating the controller. |
| PDF discovery selected an unrelated `H*.pdf` in Downloads | 2 | Select the known contest PDF by the recorded 493,976-byte size rather than a one-character prefix. |
| First combined follow-up patch did not match the current source context | 1 | Re-read the affected source blocks and applied the deadlock/emergency-stop fixes with exact context. |
| Final monitor-mode check used a combined `rg` expression with Windows path escaping | 1 | Re-run the check with fixed-string searches rather than one escaped regular expression. |
| Ozone menu coordinate opened the Find menu and `Alt+P` did not expose Project settings | 1 | Do not reuse the stale coordinate; inspect local Ozone project examples/shortcuts and re-observe before the next UI action. |
| Ozone `Ctrl+Shift+S` did not open Save Project As from the current focus | 1 | Use the already validated File menu entry instead of repeating the shortcut. |
| Ozone File-menu click used coordinates from the prior maximized layout after the window restored | 1 | Re-observed the non-maximized window and discarded all old screenshot coordinates. |
| Ozone menu click remained rejected after rebinding the restored window to its process path | 2 | Stop coordinate retry; inspect the Computer Use API and use a window-management/accessibility or keyboard route. |
| `set_value` could not write the filename in Ozone's owned Save Project As dialog | 1 | The dialog is not separately targetable; use its topmost screenshot to focus the filename field, verify focus, then type normally. |
| `Shift+F5` did not stop the session while the Debug menu was already open | 1 | Selected the visible `Stop Debug Session` menu item; Ozone disconnected cleanly. |
| First read-only mailbox request remained pending | 1 | FreeRTOS/frame inspection showed the controller was in `Task3` + `Fault`, outside the monitor-only mailbox path; reset to compiled defaults before retrying. |
| First Ozone-manual PDF search hit a GBK console encoding error | 1 | Re-ran the read-only extraction with task-local UTF-8 console output and filtered only RTOS-related lines. |
| Ozone input was repeatedly interrupted by concurrent foreground user input and the window was minimized during the authorized zero/motion test | 1 | Stopped before writing the mailbox; retain the last verified halted `Monitor/Ready`, position `0`, `zeroed=0` state and require an uninterrupted Ozone window before resuming. |
| Foreground user input was detected while typing read-only mailbox `operation=6` | 1 | Treat the operation write as unknown, stop Ozone automation immediately, and rely on the untouched unlock key/request sequence to prove no request was dispatched. |
| Ozone CPU was unexpectedly running when Watched Data was restored | 1 | Reopened the Watch read-only, verified no request could dispatch (`unlock_key=0`, sequence `0/0`), then used `Debug -> Halt` and stopped the test round. |
| Fresh unique Ozone activation failed with `no monitor found for window` after an earlier capture returned Codex instead of Ozone | 1 | Discarded all screenshots/coordinates and stopped Windows input; Ozone must be restored onto a visible monitor before another fresh selection. |
| Computer Use reported a physical Escape interruption during the fresh Ozone enumeration | 1 | Stopped the Windows automation turn before any target selection, mailbox write, zeroing, or motion command. |
| Ozone was covered by Windows Quick Settings and repeatedly minimized while Computer Use reported concurrent foreground input | 2 | Performed the single permitted reactivation/rebind recovery; it failed to keep Ozone targetable, so stopped before writing `unlock_key` or issuing any request. |
| Ozone unexpectedly restored from maximized state and the floating Watch disappeared while a ToDesk notification overlaid the desktop during speed-field editing | 1 | Obeyed the hardware-test stop-on-anomaly rule; stopped with CPU halted, request/completed 2/2, no movement request issued, target staged at 2 degrees but speed/acceleration unchanged at 30/20. |
| Fresh capture for the uniquely enumerated Ozone window returned the foreground Codex window instead of Ozone | 1 | Treated this as continuing window obstruction and stopped before activation, coordinate use, Watch edits, or target execution. |
| Phase 16 first Ozone activation timed out before state capture | 1 | Discard the stale handle, re-enumerate the unique window, and perform the single documented activation recovery without any target input. |

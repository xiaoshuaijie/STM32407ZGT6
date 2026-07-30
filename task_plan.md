# Task Plan: Vehicle Balance-Ball Firmware Integration

## Goal
Integrate the existing vision result stream, X42S stepper motor driver, LCD, two PA4/PA5 task-selection buttons, LibXR topic messaging, and resource-light command-line diagnostics into a safe STM32 balance-ball control application that follows the H-problem rules and the repository architecture.

## Current Phase
Phase 7 - Deliver

## Active Subtask
Complete `bujin_motor` TTL protocol driver updates from the ZDT X42S manual and write `bujin_motor/READMA.md`, while preserving existing balance-ball integration work.

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
| Error | Attempt | Resolution |
|---|---|---|
| Python received the Chinese PDF filename with replacement characters | 1 | Resolve the real file path through PowerShell enumeration, then pass it as an argument. |
| Baseline build fails because expanded `zdt_x42s.hpp` does not match legacy `zdt_x42s.cpp` | 1 | Complete the protocol implementation/compatibility surface before integrating the controller. |
| PDF discovery selected an unrelated `H*.pdf` in Downloads | 2 | Select the known contest PDF by the recorded 493,976-byte size rather than a one-character prefix. |
| First combined follow-up patch did not match the current source context | 1 | Re-read the affected source blocks and applied the deadlock/emergency-stop fixes with exact context. |
| Final monitor-mode check used a combined `rg` expression with Windows path escaping | 1 | Re-run the check with fixed-string searches rather than one escaped regular expression. |

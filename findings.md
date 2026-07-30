# Findings

## Current Task
- Build the STM32-side application for the H-problem vehicle balance-ball system.
- Vision sends `valid`, `x_cm`, `vx_pixel_s`, `confidence`, and `frame_time_ms` to this MCU.
- Required integrations include X42S stepper control, the existing CAN and LCD modules where useful, PA4/PA5 task controls, LibXR topics, and an on-demand command-line print path modeled after the BMI088 module.
- All conclusions below are research data; code and local documentation remain the authority for implementation decisions.

## Repository Classification
- This is an STM32F407 CubeMX/CMake LibXR platform project: evidence includes `26diansai.ioc`, generated `Core/`, STM32 HAL, `CMakeLists.txt`, `CMakePresets.json`, and `User/libxr_config.yaml`.
- It is not a full XRobot workspace: no root `Modules/`, `modules.yaml`, or `xrobot.yaml` is present.
- `xrobot-agent-context.md` directs runtime/topic work to LibXR basic-coding semantics and local implementation sources.
- The repository is broadly dirty/initially added. Existing changes and untracked modules are user work and must be preserved.
- The only `AGENTS.md` found is under the LibXR submodule; its rules must be read before inspecting or changing that subtree.

## H-Problem Requirements
- The actuator balances a steel ball in a straight 25 cm grooved PPR-tube beam; nominal controllable coordinate range is about `-12.5..+12.5 cm` around center O.
- Task 3 (car stationary): move from O to `+5 cm`, reverse, then settle at `-5 cm`; total time must be at most 5 s and maximum absolute error near each target must be at most 1 cm.
- Task 4: drive A to B within 8 s while keeping the ball around O with absolute error at most 1 cm.
- Task 5: complete a clockwise lap within 30 s while keeping the ball around O with absolute error at most 1 cm.
- Task 6: complete a clockwise lap within 30 s while keeping the ball around any specified beam position with absolute error at most 1 cm.
- The vehicle must have a start button and display. Timing starts on the start-button action; display size must not exceed 2 inches.
- Line sensing for the vehicle is restricted to infrared photoelectric modules. Ball position sensing must use a camera.
- The course is two 1.5 m straights joined by 0.5 m-radius semicircles; the line is `1.8 +/- 0.2 cm` wide.
- Task 2 requires one clockwise lap in at most 20 s and stop error at A no more than 2 cm.
- The camera/transmitter must remain on the car and cover the whole beam; remote display/recording is a separate contest requirement.
- Direct review of all four H-problem pages confirms: Task 3 is scored separately in a stationary vehicle and requires O to +5 cm then -5 cm in <=5 s with <=1 cm maximum error; Tasks 4-6 require <=1 cm error while vehicle motion is supplied by the external chassis controller. Task 4 is limited to 8 s A-to-B; Tasks 5-6 are limited to 30 s per lap.
- The rules require an on-car start button and display no larger than 2 inches. The existing 1.14-inch LCD is compliant; its display must show elapsed runtime after PA5 starts a task.
- The pypdf parser emitted a recoverable `incorrect startxref pointer` warning while extracting the contest file, but all four pages were read successfully and matched the supplied task title.

## LibXR Source Rules
- The LibXR subtree is C++ and uses PascalCase for framework source methods/types, except vendor/platform code.
- Initialization-only allocations that intentionally live for the runtime should not be mechanically freed.
- Any edits inside LibXR would require its CMake test suite; the preferred path is to use its public interfaces without modifying the submodule.

## BMI088 Topic and CLI Pattern
- Constructor creates typed topics with `LibXR::Topic::CreateTopic<decltype(data)>(name)` and stores them as `LibXR::Topic`.
- Its acquisition thread publishes with `topic.Publish(payload, sample_timestamp)`; gyro and acceleration share the data-ready interrupt timestamp in the Topic envelope rather than duplicating it in the payload.
- It registers a command endpoint using `LibXR::RamFS::CreateFile("bmi088", CommandFunc, this)` and adds it to the `ramfs` hardware object.
- The normal acquisition path performs no formatted printing. Data prints only during the finite `show <time_ms> <interval_ms>` command, so normal runtime cost is limited to topic publication.
- The command callback receives module state through the file context pointer and uses `LibXR::STDIO::Printf` plus `Thread::Sleep`; intervals are clamped.
- The balance module should follow the same separation: periodic control/topics remain print-free, while a bounded CLI `show` operation reads snapshots on demand.

## Existing Project Integration (Initial Scan)
- `User/app_main.cpp` already constructs `STM32GPIO PA4/PA5`, all three UARTs, the LCD, USB CDC terminal/RamFS, and a `Maxican::BallReceiver` on USART1.
- USART3 is intended for `BujinMotor::ZdtX42s` but its construction is currently commented out. USART2 is configured but currently unused in `app_main.cpp`.
- The current main thread starts only the vision receive thread, prints one `Hello` string, then sleeps forever; there is no control/operator loop yet.
- `26diansai.ioc` and generated GPIO code configure PA4 and PA5 as input pulldowns, so buttons should be treated as active-high unless the hardware wiring says otherwise.
- Existing `bu_task` has an early infinite tracking loop using position/velocity feedback, stale-data stop, angle limiting, and command deadband, but it has no tasks, buttons, LCD, Topic, CLI, or explicit controller lifecycle.
- The expanded X42S driver now exposes substantially more API than its early implementation. Its final public surface and current build status need focused inspection before integration.
- CMake already compiles LCD, motor, `bu_task`, and `maxican`; new balance application files can be added without modifying the LibXR submodule.
- `maxican` currently parses ASCII frames in the form `$BALL,valid,x_cm,vx_pixel_s,confidence,frame_time_ms*\r\n`; it validates numeric conversion and confidence range but currently prints every received frame, contrary to the requested opt-in diagnostics.
- `BallMailbox` keeps only the newest measurement and uses a semaphore to wake a consumer, which is appropriate for control freshness but not yet LibXR Topic communication.
- LCD text rendering writes every pixel using blocking SPI operations; the display must be updated at a low rate and outside the real-time control path.
- Baseline configure succeeds with the STM32CubeCLT Clang toolchain. Baseline build fails immediately because the expanded 37 KB motor header declares a new API while the 9 KB implementation still defines the old API.

## Task Scope
- Target driver: `bujin_motor`.
- Reference style and feature source: `ZDTMotor-master/ZDTMotor-master`.
- Protocol source: local PDF manual `ZDT_X42S第二代闭环步进电机用户手册V1.0.5_260527(1).pdf`.
- Existing code is expected to use TTL protocol; reference project mainly demonstrates CAN-oriented organization and command coverage.

## Manual Findings
- TTL/RS485 command format is `Addr Code CommandData Checksum`.
- TTL/RS485 response format is `Addr Code ResponseData Checksum`.
- Address factory default is `1`, configurable `1..255`; `0` is broadcast.
- Default checksum is fixed byte `0x6B`; manual also documents XOR, CRC8, and Modbus-RTU options.
- Common response status values: `0x02` accepted, `0x12/0x22` already at origin or limit already triggered during origin action, `0xE2` parameter/condition error, `0xEE` frame format error, `0x9F` action completed.
- CAN uses extended frames with ID `(Addr << 8) | Packet`; packet splitting is not needed for TTL, but command payload/checksum content is shared with TTL.
- Command suffix note: commands without suffix and `(X42S/Y42)` are common to X and Emm; `(X)` and `(E)` are firmware-specific.
- Action commands read so far: calibration `0x06 0x45`, reboot `0x08 0x97`, current angle zero `0x0A 0x6D`, release protection `0x0E 0x52`, factory reset `0x0F 0x5F`.
- Multi-motor TTL command uses broadcast `00 AA total_bytes_hi total_bytes_lo <embedded commands...> checksum`; embedded commands include their own address/code/payload/checksum.
- Enable command: `Addr F3 AB enable sync checksum`, response `Addr F3 status checksum`.
- X firmware velocity command: `Addr F6 dir accel_u16 speed_0.1rpm_u16 sync checksum`.
- Emm firmware velocity command: `Addr F6 dir speed_rpm_u16 accel_u8 sync checksum`.
- X direct position command: `Addr FB dir speed_0.1rpm_u16 angle_0.1deg_u32 mode sync checksum`.
- X trapezoid position command: `Addr FD dir accel_u16 decel_u16 max_speed_0.1rpm_u16 angle_0.1deg_u32 mode sync checksum`; `0xCD` adds current limit.
- Emm position command: `Addr FD dir speed_rpm_u16 accel_u8 pulses_u32 mode sync checksum`.
- Emm fast position: configure `Addr F1 speed_rpm_u16 accel_u8 mode sync checksum`; execute `Addr FC signed_pulses_i32 checksum`.
- X fast trapezoid position: configure `Addr F1 accel_u16 decel_u16 max_speed_0.1rpm_u16 mode sync current_ma_u16 checksum`; execute `Addr FC signed_angle_0.1deg_i32 checksum`.
- Stop command: `Addr FE 98 sync checksum`; trigger synchronized motion: `Addr FF 66 checksum`.
- Origin commands: set single-turn zero `0x93 0x88 save`, trigger origin `0x9A mode sync`, interrupt origin `0x9C 0x48`, read origin status `0x3B`, read origin params `0x22`, modify origin params `0x4C 0xAE save ...`.
- Common read commands include firmware/hardware version `0x1F`, phase resistance/inductance `0x20`, bus voltage `0x24`, bus current `0x26`, phase current `0x27`, encoder linearized `0x31`, input pulses `0x32`, target position `0x33`, set target position `0x34`, realtime speed `0x35`, realtime position `0x36`, position error `0x37`, temperature `0x39`, system flags `0x3A`, origin+system flags `0x3C`, pin state `0x3D`, battery voltage `0x38`.
- Position/speed decoding differs by firmware: Emm position values use 0..65535 per single turn for reads and pulses for Emm position commands; X position/angle commands use 0.1 degree units, and X speed reads use 0.1 RPM.
- Parameter write commands include motor ID `0xAE 0x4B`, microstep `0x84 0x8A`, power-loss flag `0x50`, motor type `0xD7 0x35`, firmware type `0xD5 0x69`, control mode `0x46 0x69`, positive direction `0xD4 0x60`, button lock `0xD0 0xB3`, scale input `0x4F 0x71`, open-loop current `0x44 0x33`, FOC current `0x45 0x66`, position window `0xD1 0x07`, heartbeat `0x68 0x38`, stiffness/integral limit `0x4B 0x57`, collision-origin return angle `0x5C 0xAC`, and parameter lock level `0xD6 0x4B`.
- Autostart speed storage uses `0xF7 0x1C`; X and Emm formats differ in speed/acceleration units.
- Bulk system state read `0x43 0x7A` differs by firmware: X response is 37 bytes with 12 params; Emm response is 31 bytes with 9 params.
- Driver config read `0x42 0x6C` differs by firmware: X response is 37 bytes with 24 params; Emm response is 33 bytes with 21 params.
- CRC8 checksum starts with the first frame byte, then indexes a 256-byte table by `crc ^ next_byte`; this matches the reference library's `UpdateCRC8` helper.

## Code Findings
- `bujin_motor` currently exposes a small C++ wrapper around `LibXR::UART` with fixed-6B or XOR checksum support.
- Current TTL coverage: enable `0xF3`, immediate stop `0xFE`, set current position as zero `0x0A`, absolute position move `0xFD`, and realtime position read `0x36`.
- Current frame helpers already use big-endian integer packing and synchronous LibXR UART read/write operations.
- Current acknowledgement handling expects 4-byte frames `{addr, command, status, checksum}` and maps status `0x02`, `0x9F`, `0x12/0x22`, `0xE2`, `0xEE`.
- `ZDTMotor-master/ZDTMotor.hpp` style centralizes command constants/enums, validation helpers, typed parameter structs, checksum calculation, command builders, and response decoders.
- Reference command coverage includes calibration, clear stall, reset zero, unblock, factory restore, enable, speed, position, fast position, stop, sync movement, origin operations, system parameter reads, parameter modifications, and response decoding.
- Project usages found outside `bujin_motor`: `User/app_main.cpp`, `bu_task/bu_task.hpp`, `bu_task/bu_task.cpp`, and `jie.cpp`.

## Design Decisions
| Decision | Rationale |
|---|---|
| Controller state owns all motor commands | Keeps USART3 request/reply transactions serialized and makes an abort from the UI a safe state transition rather than a concurrent motor write. |
| Task 3 advances after 300 ms within 1 cm | Prevents one noisy vision frame from counting as a settled target. |
| Tasks 4/5/6 use 8/30/30 s controller timeouts | The balance module can enforce its own contest time limit even though it does not drive the chassis. |
| Task 6 target is a named, compile-time calibration constant | The task's specified point is selected before the run; changing one safe configuration value avoids overloading the two-button contest UI. |

## Verification Findings
- `cmake --build --preset Debug` succeeds after adding the receiver Topic/terminal command, control state machine, operator panel, and application wiring.
- Final linked image usage: RAM 50,056 B / 128 KB (38.19%); flash 125,580 B / 1 MB (11.98%).
- Scoped source whitespace validation reports no errors. The full repository check still reports only the pre-existing unrelated blank line at `Core/Src/freertos.c:129`.
- Manual review found and fixed a potential self-deadlock: task 3 phase transition now releases the controller mutex before publishing `balance_state`.
- Hardware validation remains required for X42S direction, level angle offset, pulse count, closed-loop response, and vision-to-angle gains.

## Revised MaixCAM UART Protocol (2026-07-30)
- The legacy `$BALL,valid,x_cm,vx_pixel_s,confidence,frame_time_ms*` ASCII protocol has been replaced by a fixed 13-byte binary frame.
- Frame layout: bytes `0..1` are `0xAA, 0x55`; byte `2` is `valid`; bytes `3..4` are signed little-endian `x_cm_x100`; bytes `5..6` are signed little-endian `vx_px_s_x10`; byte `7` is `confidence_pct`; bytes `8..11` are little-endian `frame_time_ms`; byte `12` is the XOR of bytes `0..11`.
- Example valid frame: `AA 55 01 D2 04 C9 FD 58 4E 61 BC 00 D7`, corresponding to `12.34 cm`, `-56.7 px/s`, confidence `88%`, and source time `12345678 ms`.
- `valid == 0` denotes lost target; STM32 must disregard the position, velocity, and confidence payload values. The public receiver measurement should therefore expose neutral zero values for those fields while retaining the received timestamp and invalid state.
- `maxican` now accepts only checksum-valid 13-byte frames, converts valid fixed-point readings to cm and px/s, and publishes zero-valued measurements for valid target-loss frames. A checksum failure retains a candidate trailing `AA 55` header so the byte stream can resynchronize without waiting for an unrelated reset.
- The documented test vector calculates to XOR `D7`, `12.34 cm`, `-56.7 px/s`, confidence `0.88`, and source time `12345678 ms`. `cmake --build --preset Debug` passed after the receiver update.

## Terminal Diagnostic Regression (2026-07-30)
- The supplied terminal capture shows `ball show 10000 100` being rejected with `Error: time_ms and interval_ms must be positive integers.` even though both supplied values are positive. The `BALL +4.35 cm` overlay confirms vision reception itself is operating, so the fault is localized to RamFS command argument handling.
- `BallReceiver::CommandFunc` receives the expected `argc == 4` for this command and delegates both numeric fields to `ParseCommandMilliseconds`; the displayed error can only result from that parser failing or the duration resolving to zero.
- The vision parser proves `std::strtoul` itself works on this target. The command-specific helper additionally relies on C-string termination; RamFS token storage is now the likely incompatibility. The correction should use bounded digit parsing suitable for the command buffer, and should validate both `duration_ms` and `interval_ms` as nonzero before clamping.
- RamFS does pass writable C strings to commands. LibXR's terminal handles both `CR` and `LF` as control characters before command execution, so a retained CRLF suffix is not the cause. The screenshot establishes only that the libc `strtoul` path fails in the board command callback. Replace it with a deterministic digit-by-digit decimal parser, retaining strict rejection of non-digits/overflow and validating both values as nonzero.
- The supplied `$BALL,1,<x_cm>,<vx_pixel_s>,<confidence>,<frame_time_ms>*` records match the receiver protocol exactly. Values including `x_cm=3.77..10.68`, finite velocity, confidence `0.75/0.90`, and unsigned timestamps are valid. The UART transport must still append a newline after `*` so `BallReceiver::Run()` finalizes the frame.

## Terminal Diagnostic Fix Verification (2026-07-30)
- `ParseCommandMilliseconds` now accumulates only ASCII decimal digits and checks `uint32_t` overflow before every multiplication. It no longer uses `strtoul` in the RamFS callback.
- `ball show 10000 100` resolves to a 10,000 ms duration and 100 ms print interval. Both zero duration and zero interval are rejected; existing duration and interval caps remain unchanged.
- `cmake --build --preset Debug` passed. Linked image: RAM 50,056 B / 128 KB (38.19%); flash 125,548 B / 1 MB (11.97%).

## Vision UART Reception Diagnosis (2026-07-30)
- `BallReceiver` is constructed with `usart1`, while the user-facing RamFS terminal uses `usart2`. The frame receiver therefore listens only on USART1, not on the UART2 terminal pins.
- CubeMX config maps USART1 TX/RX to PA9/PA10. USART1 RX uses DMA2 Stream2 in circular mode and USART1_IRQn is enabled, so the project configuration expects vision-board TX to connect to PA10.
- The visual sender frame grammar is accepted by `ParseFrame` when bytes reach USART1; continue with USART1 runtime initialization and baud verification before attributing the issue to the parser.

## Session Recovery: 2026-07-30 Balance Integration
- The active root plan already covers the user's complete request. Existing uncommitted work includes UART2 CubeMX configuration, early vision/motor/thread integration, and an expanded but not yet implemented X42S driver API; treat all of it as user-owned work to preserve.
- The implementation target remains the STM32F407 LibXR platform. The application must use public LibXR services and must not modify the LibXR submodule unless that becomes unavoidable.
- Current integration starts a vision receiver and an early tracking loop but has not yet implemented contest mode selection, LCD status refresh, Topic publication/consumption, or bounded CLI diagnostics.
- The isolated serial-driver plan reports that the header/implementation mismatch was reconciled and a Debug build passed at that point. The current source still needs a fresh whole-project build after the broader balance integration is added.
- `maxican` correctly bounds/parses the required `$BALL` line and coalesces fresh measurements, but it currently calls `STDIO::Printf` for every received frame. This must move to an explicit finite CLI `show` path.
- `bu_task` currently exposes only a continuous motor-position tracking loop. It has no application lifecycle, task trajectory state machine, button debouncing, LCD state model, or LibXR Topic contract.

## Authoritative Interface Review
- The BMI088 module confirms the intended pattern: create a typed `LibXR::Topic` in the module constructor, publish acquisition data with a timestamp, register a RamFS command file, and perform formatted output only from finite command operations such as `show`.
- The reference ZDT CAN class is useful for its enum/validation/decoder organization, but must not be copied into `bujin_motor`: this project uses the X42S TTL UART framing and synchronous `LibXR::UART` transactions.
- The checked LibXR contribution rules apply only inside the submodule. Planned changes remain in project-owned modules and use their existing C++ naming conventions.
- Contest scope assigned to this firmware: Task 3 needs a timed +5 cm then -5 cm setpoint sequence; Tasks 4 and 5 hold 0 cm while external vehicle motion runs; Task 6 holds a configurable specified position. Vehicle navigation/line following remains external to this balance subsystem.
- PA4/PA5 are generated as active-high pulldown inputs. The operator model will use PA4 as task select and PA5 as start/abort, with software debounce in a low-rate UI thread.
- The LCD has an ASCII-only font and blocking SPI pixel writes. It must be refreshed at a bounded low rate with compact ASCII status lines; the control loop must never draw it.
- BMI088 registers a command with `RamFS::CreateFile("bmi088", CommandFunc, this)` then `ramfs.Add(...)`; the balance modules can follow this exactly without modifying LibXR.
- LibXR Topics synchronously dispatch each publish but intentionally do not cache the latest payload. `BallMailbox` remains necessary for the controller's latest-value/timeout semantics, while the `ball_measurement` Topic supplies the requested publish-subscribe integration for other consumers.
- `BallMeasurement` and the planned control-state payload satisfy LibXR's typed Topic contract (default constructible, copy-assignable, trivially destructible POD data).
- `STM32GPIO::Read()` is a direct active-high pin-level read, so a 20 ms polling/debounce worker is sufficient for PA4/PA5.

## Errors Encountered
| Error | Attempt | Resolution |
|---|---:|---|
| Direct Python source literal for the Chinese H-problem filename was corrupted by the active console code page | 2 | Resolve the PDF path in PowerShell by its ASCII prefix/suffix and pass it through an environment variable or a temporary argument-free discovery step. |
| A broad `H*.pdf` search selected an unrelated HAL tutorial PDF | 3 | Find the contest document by its known 493,976-byte length and validate the page title before extracting it. |
| Assumed obsolete `src/system` paths for RamFS/Timebase/Thread headers | 1 | Use the paths resolved by `rg` under `src/middleware/ramfs` and the umbrella `libxr.hpp`; no framework source edit is needed. |
| A broad follow-up patch did not match the current source text | 1 | Re-read exact contexts, then applied a smaller patch for the deadlock and stop-path corrections. |

## Errors Encountered
| Error | Attempt | Resolution |
|---|---|---|
| Used Bash here-doc syntax in PowerShell while probing Python modules | 1 | Re-ran the probe with a PowerShell here-string. |
| Console could not encode one PDF page character while printing extracted text | 1 | Continue with UTF-8/replacement-safe output. |
| Direct inline Python path for the Chinese H-problem filename was corrupted by the console code page | 1 | Resolved the actual path in PowerShell and passed it as a positional argument. |

# Findings

## Phase 16 continuation boundary (2026-07-31)
- Planning recovery found no pending unsynchronized context.
- The observed approximately 25.3% travel is not, by itself, proof of a 4x pulse-scale error. The live conversion must remain unchanged until the `0x42 0x6C` read-only configuration response or equivalent hardware documentation independently establishes motor type, microstep, and pulses-per-revolution/transmission assumptions.
- The diagnostic mailbox belongs to the newly loaded firmware and is reset; it must not be interpreted as proof that the physical shaft returned to zero.
- `ZdtX42s::Config` currently defaults to address `1` and `3200` pulses/revolution. `MoveToAbsoluteAngle` converts degrees to Emm command pulses using `pulses_per_revolution / 360`.
- Repository search found no configured gear/transmission ratio. `bujin_motor/READMA.md` documents the current `3200` value as an assumption for a 1.8-degree motor at 16 microsteps and explicitly requires adjustment to the physical motor.
- `User/app_main.cpp` constructs the motor with `ZdtX42s motor(usart3)`, so the active motor ID/address is the default `1`; there is no call-site override for address, direction, PPR, or a transmission ratio.
- `MotorDebugOperation::ReadMotorConfig` is guarded by the existing unlock key, sequence, and Monitor/not-Running checks. Its branch only calls `ReadMotorConfig`, copies decoded fields, and completes the mailbox request; it cannot enable, zero, or move the motor.
- The implemented Emm configuration offsets are: motor type byte 4, pulse-port mode 5, communication-port mode 6, enable level 7, positive direction 8, microstep 9, interpolation 10, serial baud selector 18, address 20, checksum mode 21, response mode 22, and position window big-endian bytes 30..31. The final byte is checksum.
- PPR derivation recognizes manual motor-type codes `0x19` as 200 full steps/rev (1.8 degrees) and `0x32` as 400 full steps/rev (0.9 degrees); microstep byte zero is interpreted as 256.
- Local protocol sources were found under `C:\Users\24137\xwechat_files\wxid_lr4ikrpn2i0q12_ee58\msg\file\2026-07`, including `X42S_Guide(2).pdf` and the second-generation ZDT X42S manual. These can be read directly without another full-profile scan.
- The Windows-control runtime requires an exactly unique returned Ozone window before activation. Every input must be derived from a fresh observation and followed by a refresh; any user interruption or uncertain result invalidates the prior window state.
- The current Ozone session cannot provide read-only motor configuration evidence because its first fresh capture showed a target-connection-loss modal. The prior halted state must no longer be assumed current, even though no mailbox request was sent in this continuation.
- The failed 2-degree trial sent `round(2 * 3200 / 360) = 18` pulses. Its `0.505371094`-degree readback equals 92 encoder counts at 65536 counts/revolution. Candidate PPR 12,800 predicts 0.50625 degrees and the same quantized 92 counts, making it strongly consistent with the observation but not uniquely identifying motor type/microstep.
- A 12,800-PPR motor configuration can arise from at least `0x19` (1.8-degree/200-step) with 64 microsteps or `0x32` (0.9-degree/400-step) with 32 microsteps. External 4:1 transmission is another application-level scaling possibility if the requested angle is an output-shaft angle, but it would not explain the internal motor encoder readback by itself.
- Motor ID/address `1` is strongly supported by the previously successful realtime-position transactions because the driver rejects responses whose first byte is not the configured address. The live config read is still needed to independently confirm the stored address and other parameters.
- Hardware evidence required before changing PPR: a successful `0x42 0x6C` readback containing firmware family, motor type, microstep, address, checksum mode, and response mode; alternatively, a clear motor/driver display or parameter screenshot showing step angle and microstep. If the requested angle is measured after a gearbox/belt linkage, tooth counts or the exact input:output transmission ratio are also required.
- The previous firmware's positive `0.505371094` display was produced with the reversed sign constants. Under the manual's `00=positive, 01=negative` definition, that response is likely negative. Treat the old direction conclusion as invalid; the scaling candidate uses absolute magnitude only.
- The authoritative V1.0.5 manual is 8,673,480 bytes and is distinct from the shorter `X42S_Guide(2).pdf`; protocol offsets will be checked against the V1.0.5 manual.
- Manual pages 103 and 106 confirm the exact Emm configuration layout and example `01 42 21 15 19 ... 10 ... 01 00 01 ... 00 08 6B`: motor type `0x19` is 1.8 degrees, microstep `0x10` is 16, address is `0x01`, checksum selector is `0x00` (fixed `0x6B`), and response mode is `0x01` (Receive).
- Manual page 98 defines realtime position as a signed 32-bit magnitude scaled by `360/65536` degrees. It states sign `00/01` means positive/negative respectively and gives `131072 -> 720 degrees`; current `ReadRealtimeAngle` reverses those signs and rejects all magnitudes above 65535. This is evidence-backed and must be corrected before direction-sensitive validation.
- Manual page 57 confirms the Emm `FD` format implemented by `MoveToAbsoluteAngle`: direction, big-endian RPM, one-byte acceleration, big-endian pulse count, motion mode, sync flag. Absolute mode is `01`; immediate execution is `00`.
- The angle-to-command formula is valid only with actual PPR. The manual's `3200` is explicitly an example for 1.8-degree motor and 16 microsteps, not a universal motor constant.
- Apart from the imported reference project's tests, this firmware has no project-owned unit-test executable. Focused driver math is currently verified using anonymous-namespace `constexpr` helpers plus `static_assert`, which can cover signed position counts without introducing a new test framework.

## X42S motion-scaling diagnosis (2026-07-31)
- `User/app_main.cpp` constructs `ZdtX42s motor(usart3)` with the default config, so the live firmware uses address `1`, fixed `0x6B` checksum, positive direction counterclockwise, and `pulses_per_revolution=3200`.
- `MoveToAbsoluteAngle` currently emits the Emm-style 13-byte `FD` frame and converts degrees to an integer pulse count using `degrees * pulses_per_revolution / 360`; the `+2` degree test therefore commanded 18 pulses after rounding.
- `ReadRealtimeAngle` expects the Emm-style 8-byte `0x36` response and converts the 32-bit magnitude from a 65536-count revolution to degrees. Successful repeated feedback proves that this response layout is accepted by the connected device, but does not by itself prove its configured input-pulse microstep.
- There are no project-local unit tests for this TTL driver. The only existing motor tests cover the separate reference `ZDTMotor-master` implementation.
- The reference implementation defines the read-only motor-config command as code `0x42` with subcode `0x6C`. Its Emm decoder expects a 30-byte payload beginning with `total_bytes=0x21` and `parameter_count=0x15`; `microstep` is payload index 7. The current TTL driver has no equivalent API, so this must be verified against the local manual before adding a bounded read-only query.
- The local V1.0.5 X42S manual was found at `C:\Users\24137\xwechat_files\wxid_lr4ikrpn2i0q12_ee58\msg\file\2026-07\ZDT_X42S第二代闭环步进电机用户手册V1.0.5_260527(1).pdf`.
- Manual pages 16, 57, 70, and 76 state that an Emm 1.8-degree motor at 16 microsteps uses 3200 input pulses per revolution; 32 microsteps uses 6400. Microstep values `1..255` encode directly and `0` means 256.
- Manual pages 102 and 106 explicitly show the read-only request `01 42 6C 6B` for address 1/fixed checksum. This independently confirms the command identified in the reference implementation.
- Manual pages 103-106 define the Emm reply as exactly 33 bytes: address, `0x42`, `0x21` total bytes, `0x15` parameters, then 28 configuration data bytes and checksum. In the complete frame, motor type is index 4, microstep index 9, interpolation index 10, baud-rate index 18, address index 20, checksum mode index 21, response mode index 22, and position window is big-endian at indexes 30-31.
- The manual example reply is `01 42 21 15 19 02 02 02 00 10 01 00 04 B0 0B 80 0F A0 05 07 01 00 01 01 00 08 08 98 07 D0 00 08 6B`, confirming 1.8-degree motor type `0x19`, 16 microsteps, address 1, fixed-6B checksum, and Receive-only acknowledgements.
- This exact layout makes a bounded read-only TTL API possible. The production pulse scale must remain unchanged until the connected device returns its own microstep value successfully.
- The existing Ozone mailbox has no configuration-read operation, but it already enforces `Monitor`, an unlock key, serialized sequence numbers, and exclusive controller ownership of USART3. A new read-config operation can reuse these gates without calling `Enable`, zeroing, or any motion API.
- The Ozone project adds Watch expressions by symbol name, so extending the mailbox and adding new explicit config fields is robust to the mailbox address changing after rebuild.

## X42S Ozone continuation state (2026-07-31)
- A fresh window enumeration returned one and only one `26diansai.elf` Ozone target (`id=1579548`).
- The latest screenshot confirms CPU halted, the Watch window visible without obstruction, `Monitor/Ready`, request/completed `2/2`, successful result and valid `0` degree telemetry, `zeroed=1`, and no active motion.
- The conservative motion fields are already staged as target `2` degrees, speed `1` RPM, and acceleration `1`; only the MoveAbsolute operation/unlock/request sequence remain to be committed.
- The first `+2` degree MoveAbsolute request was consumed (`completed_sequence=3`) but returned mailbox `result=-12`, which LibXR defines as `TIMEOUT`; it stopped after 51 position samples at `0.505371094` degrees with `1.49462891` degrees remaining error. USART3 telemetry stayed valid and controller state stayed `Monitor/Ready`, with `motion_active=0` after completion.
- The timeout path calls `StopImmediately()` before clearing `motion_active`, so the final stopped state is enforced by firmware rather than inferred only from the debugger halt.
- The driver currently uses its default `pulses_per_revolution=3200`, and its README explicitly requires matching this value to the physical motor microstep setting. Reaching about 25.3% of the commanded angle is consistent with a roughly 4x pulse-scale mismatch, but the actual motor configuration register was not read, so this remains the leading hypothesis rather than a confirmed root cause.

## Current Hardware Test (2026-07-31)
- The user reports the stepper motor is powered and connected to USART3 and requests programming/debugging through SEGGER Ozone V3.40g.
- Safety policy for this test: obtain valid position telemetry before motion; begin with low-speed, small-angle commands because mechanical travel, direction, and limit-switch state are not yet known; stop immediately on missing feedback, protocol error, or unexpected motion.
- Acceptance criteria: USART3 returns valid X42S position data; commanded targets are reached with bounded position error; motion is visually/telemetrically continuous at conservative speed and acceleration; the motor is left stopped or at a documented safe position.
- Windows currently enumerates only Bluetooth virtual ports COM3 and COM4; no USB serial adapter is visible. USART3 is an MCU-to-X42S link and can still be tested through firmware state in Ozone.
- No existing `.jdebug`, `.elf`, `.hex`, or `.bin` was found by the initial repository-wide file scan. Ozone project setup and a fresh Debug build are required.
- The current firmware already constructs `ZdtX42s` on USART3 (PD8 TX, PD9 RX, 115200 8N1) and the default `VIEW` mode polls `ReadRealtimeAngle` every 500 ms without commanding motion.
- `BalanceController` is designed as the sole USART3 owner; hardware test commands must use that owner or a dedicated mutually exclusive test mode, not a second concurrent UART client.
- Ozone V3.40g was already running in a single `*New Project` window. It has loaded this firmware's FreeRTOS/LibXR sources and symbols, but the status bar is `Disconnected`; the current session should be inspected before creating or overwriting a project.
- The MCU is `STM32F407VGT6`. USART3 is PD8 TX / PD9 RX with DMA1 Stream3 / Stream1 and an enabled USART3 IRQ.
- No user or installation Ozone `.jdebug` example was found. The exact project should therefore be created through Ozone's wizard or a validated local project format, not guessed from an unrelated target.
- A safe debugger command mailbox is feasible inside `BalanceController`: it can be consumed only in `Monitor`, keeping USART3 single-owner semantics. Proposed limits are +/-10 degrees, 30 RPM default, conservative acceleration, explicit sequence numbers, and result/position globals for Ozone observation.
- Windows detects a present SEGGER J-Link (`USB\\VID_1366&PID_1020`, serial `000802008886`). Ozone can therefore use a real J-Link connection rather than an unavailable adapter.
- The installed Ozone manual is `C:\\Program Files\\SEGGER\\Ozone\\UM08025_Ozone.pdf`. The running Ozone File menu exposes New/Open/Edit Project File; a fresh wizard project can replace the unknown unsaved settings.
- Ozone's project wizard resolved the target as `STM32F407VG` with Cortex-M4/FPU and internal flash. The connection page shows SWD at 4 MHz over USB and the expected J-Link serial `802008886`; no connection-setting change is required.
- The Ozone program file is the current `build/Debug/26diansai.elf`. Optional startup defaults are correct: ELF entry point for PC and vector table for the initial stack pointer.
- Ozone diagnosed the ELF as FreeRTOS-based and offered the matching `FreeRTOSPlugin` project fixup. The fixup was applied so task stacks and RTOS state can be inspected while debugging.
- The validated Ozone project is now saved at the repository root as `26diansai.jdebug`.
- The Ozone-only motor mailbox is integrated into the existing controller thread. It is inactive after reset, accepts commands only in `VIEW`, requires unlock key `0x58423432`, requires an explicit zero operation before movement, limits targets to +/-10 degrees, speed to 60 RPM, and acceleration to 50, and auto-stops after three 50 ms samples within +/-0.5 degrees or a 3 s timeout.
- The first Debug build with the mailbox linked successfully at 50,760 B RAM and 126,176 B flash. One deprecated volatile-increment warning was identified and replaced with explicit read/write before final rebuild.
- The clean final Debug build is 50,760 B RAM (38.73%) and 126,176 B flash (12.03%). `g_motor_debug_mailbox` is a 60-byte global at `0x200000A4`, which gives Ozone a stable symbol and address.
- Protocol risk to verify before movement: the existing driver sends the Emm-style `FD` pulse-count frame and decodes `36` feedback as 0..65535 counts per revolution. The local manual research also describes an X-firmware 0.1-degree variant; actual hardware feedback must decide which format is present before zeroing or moving.
- Ozone connected to the STM32 through J-Link/SWD, initialized the DAP, downloaded the final ELF to flash, verified it, and halted at `HAL_Init()` with a live debug session.
- The flashed application ran for several seconds in default `VIEW` and halted cleanly inside an LCD SPI call. FreeRTOS/application execution is alive and no HardFault occurred before the explicit halt.
- Ozone manual section 4.23 confirms the Watched Data `Add` command uses `Alt+Shift+Plus` and supports live updates at 1-5 Hz. This is the supported route for adding `g_motor_debug_mailbox`; blank-row clicking is unnecessary.
- The full mailbox expression was added but its structure members did not expand in the compact pane. The project now uses the documented `Watch.Add` API to add each command/result field as an independent editable row on every project load.
- After `Attach & Halt`, Ozone read the mailbox reset state exactly as designed: unlock/operation/request/completed are 0, result is the pending sentinel `-2147483648`, and position validity is 0. No command has executed yet.
- The first read-only request was deliberately not consumed: Ozone's FreeRTOS task view confirmed `balance_ctrl` is alive, while frame-local inspection showed `status_.selected_task == Task3` and `status_.run_state == Fault`. The mailbox handler is only called in `Monitor`, so the request remained pending and no motor command was issued.
- The safe recovery is a target reset back to the firmware defaults (`Monitor`/`Ready` and a cleared mailbox), followed by a fresh read-only request before any zeroing or movement.
- The controller state was recovered in place through Ozone to `selected_task = Monitor (0)` and `run_state = Ready (0)`. The existing single `ReadPosition` request remained at sequence 1; no second request was created.
- After approximately 0.8 s of target execution, `completed_sequence` advanced to 1, matching `request_sequence = 1`; `result = 0`, `position_valid = 1`, and `position_degrees = 0`. This is a finite, in-range position response accepted by the X42S driver, so USART3, the configured motor address, and the implemented response format/checksum path are working for position telemetry.
- The target was halted immediately after the read-only request. No zeroing, enable-test, or movement operation was issued.
- Before the requested small-angle motion test, Ozone reconfirmed `selected_task = Monitor (0)`, `run_state = Ready (0)`, and the accepted encoder position remained `0` degrees.
- The motion test is blocked by the existing mailbox safety contract: `BalanceController::motor_debug_zeroed_` is still false. `MoveAbsolute` checks this flag before calling `Enable` or `MoveToAbsoluteAngle` and returns `STATE_ERR` when it is false. The current instruction prohibits the only supported flag-setting operation (`SetCurrentPositionZero`), so no motion command can safely reach the motor through this mailbox without either zeroing or changing/bypassing the interlock.
- No interlock flag was forged in Ozone, no movement request was issued, and the target remains halted with no active mailbox motion.
- On the 2026-07-31 resume, `list_apps()` again returned exactly one Ozone window (`id=1579548`, `26diansai.elf`). Its fresh screenshot reconfirmed CPU Halted and the visible independent Watch values `unlock_key=0`, `operation=2`, `request_sequence=1`, `completed_sequence=1`, `result=0`, and `position_valid=1`.
- Before any field edit, a Watched Data focus click was blocked because Windows Quick Settings covered the target point. Ozone was then minimized while the runtime reported concurrent foreground input; the one allowed activate/rebind recovery did not hold. No mailbox write or target execution occurred.
- After the user restored Ozone, the unique session was recovered and halted through Debug > Halt. The independent Watch preconditions were `Monitor/Ready`, operation 2, unlock 0, request/completed 1/1, result 0, valid position 0 degrees.
- The authorized zero request used unlock `0x58423432` (Watch decimal readback `1,480,733,746`) and sequence 2. After execution and halt, request/completed were 2/2, result 0, position valid 1 at 0 degrees, mailbox zeroed 1, motion active 0, and status remained `Monitor/Ready`; the handler cleared unlock back to 0 as designed.
- Source verification at `balance_controller.cpp:447-452` confirms a successful zero operation sets internal `motor_debug_zeroed_ = true` before publishing mailbox `zeroed = 1`; the mailbox readback therefore proves the movement interlock is open.
- Added independent Watch expressions for `target_degrees`, `speed_rpm`, and `acceleration`; initial readbacks were 0 degrees, 30 RPM, and acceleration 20. The target field was changed and read back as 2 degrees while halted.
- Before speed could be changed, Ozone unexpectedly left its maximized layout, the floating Watch closed/disappeared, and a ToDesk notification overlaid the lower-right desktop. Per the explicit stop-on-any-anomaly requirement, no further UI input was issued. The docked Watch still showed CPU halted, operation 2, unlock 0, request/completed 2/2, result 0, and position valid 1. No movement sequence was created.
- After a clean sole-executor resume, the independent motion fields were safely staged and verified while halted as target 2 degrees, speed 1 RPM, and acceleration 1. The mailbox remains at operation 2, unlock 0, request/completed 2/2 until the final movement trigger fields are committed.

## Current Task
- Improve the LCD operator-panel layout: the photographed 1.47-inch display shows small status text crowded against the left edge and a long row being clipped. The requested outcome is larger text moved toward the right side while retaining all status information.
- The panel's `FONT_2412` geometry is 12x24 pixels. With x=80, the longest current 15-character status row occupies 180 pixels and ends at x=260; five rows at y=10, 42, 74, 106, and 138 end at y=162, within the 320x172 display.
- The Debug firmware rebuild succeeded after the panel update: RAM 50,696 B / 128 KB (38.68%), flash 123,620 B / 1 MB (11.79%).
- `ZdtX42s::ReadRealtimeAngle` is the X42S TTL encoder query. It is synchronous and must remain in the controller task, which is already the sole owner of USART3; the LCD task must only render the copied `BalanceStatus` data.
- The controller queries the encoder every 500 ms while running. The LCD row is `M<Y/N><actual> G<target>` in degrees; six 12x24 rows at y=0, 29, 58, 87, 116, and 145 end at y=169 within the 320x172 display.
- The Debug firmware rebuilt with telemetry enabled: RAM 50,696 B / 128 KB (38.68%), flash 124,156 B / 1 MB (11.84%).
- The existing non-running path only sleeps, so LCD values do not update before a task starts. The new default monitor mode must refresh the measurement/encoder fields without passing through `StartTask`, which would enable, zero, or move the motor.
- `ContestTask::Monitor` is now the default selection and renders as `VIEW READY`. The PA4 sequence is `VIEW -> T3 -> T4 -> T5 -> T6 -> VIEW`; PA5 is ignored in `VIEW`. Its refresh path reads only the vision mailbox and X42S encoder status, without enable, zero, move, or stop commands.
- The Debug firmware rebuild succeeded: RAM 50,696 B / 128 KB (38.68%), flash 124,588 B / 1 MB (11.88%).
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
- The `VIEW` selection currently ignores PA5 because `ToggleRun()` only queues a run when the selected task is not `Monitor`; its status loop is telemetry-only. A bidirectional test therefore needs a separate monitor run path rather than reuse the vision-dependent contest controller.
- PA4/PA5 now use pull-ups and are sampled as active-low in the operator panel, so an unpressed button cannot start a test at boot.
- The implemented VIEW self-test is independent of vision data: PA5 enables and zeroes the motor, commands `+10` then `-10` degrees at 30 RPM/acceleration 20, polls the encoder at 50 ms, accepts 0.5-degree position error, and faults after 3 s per direction. The normal abort and completion paths both call `StopImmediately()`.
- While `VIEW` is running, PA4 no longer changes task selection; it queues a reverse request. The controller consumes that request in its USART3-owning thread and commands `-10°`, preserving serialized motor access. PA4 retains task selection behavior in `VIEW READY`.
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
## Phase 16 resumed Ozone state (2026-07-31)
- After context recovery, a fresh unique-window selection and screenshot again prove the target is halted and connected with the diagnostic mailbox at its reset baseline. `config_pulses_per_revolution` resolves at `0x20000CF4`; its zero value is only the mailbox reset value and is not motor-configuration evidence until the read-only request completes.
- `config_address` resolves at `0x20000CF8` and likewise remains a reset zero until a successful read-only request. Detected foreground input interrupted the attempt to stage `operation=6`; because the unlock key and request sequence were not written, the firmware cannot dispatch any request even if the operation value changed.
- A later uninterrupted retry proved `operation=6` can be staged while halted. The second foreground interruption occurred before selecting `unlock_key`; visible `unlock_key=0` and `request_sequence=0` prove no query was dispatched.
- The Ozone CPU later entered running state without an authorized Continue action in this continuation. Because unlock remained zero and request sequence remained zero, this could not dispatch the diagnostic operation. Manual `Debug -> Halt` restored a verified halted state; the unexpected run transition remains an Ozone/UI-session anomaly, not a motor communication result.
- The requested 16-degree absolute target exceeds the firmware's current diagnostic bound `kMotorDebugMaxAbsTargetDegrees = 10.0F`; a minimum scoped firmware change is required even after scaling is proven.
- Ozone could not be rebound after its capture returned the wrong foreground window: the fresh unique handle failed with `no monitor found for window`. This is a Windows display/window targeting blocker, not a target fault or USART3 result.
- Ozone later became targetable and the read-only request was staged successfully as sequence 1. The interruption occurred before Continue, so configuration results remain unavailable. If execution resumes externally, sequence 1 can invoke only `ReadMotorConfig`; no motion operation is staged.
- On the new test resume, the unique Ozone session is connected again at 4 MHz but initially shows CPU running. Installed Ozone documentation maps `F6` to `Debug.Halt`, providing a deterministic halt action.
- The Ozone manual contains a context-dependent F6 conflict: debugging controls use it for Halt, while debug information windows use it for focus traversal. Because the source pane was focused and CPU remained visibly running, a toolbar/menu action is required for an unambiguous halt.
- The documented `Debug -> Continue/Halt` menu path halted the connected CPU successfully. This establishes a new reliable debugger state, but not yet a reliable application/mailbox state because Watch expressions are absent and the latest ELF has not been explicitly downloaded.
- The corrected ELF is now explicitly downloaded and the reset mailbox proves `Monitor/Ready`, `0/0`, no motion, no zero, and no valid configuration. Saved Watch expressions survived as window content even though the old floating window had closed.
- A fresh Windows enumeration returns exactly one target Ozone session: `SEGGER Ozone V3.40g ... 26diansai.elf`, window id `1579548`.
- The floating `Watched Data 1` window is present and shows the diagnostic firmware's reset mailbox: Monitor/Ready, sequence `0/0`, Pending result, `position_valid=0`, `motor_debug_zeroed_=0`, and `motion_active=0`.
- This proves the previous firmware's `3/3, TIMEOUT, +0.505371094 degree` record is not being confused with the diagnostic firmware mailbox. The motor's physical shaft may still be at that prior safe position, but the new firmware has not queried it yet.
- Ozone's installed `ReleaseOzone.html` states that the last Watched Data table row is an input field and that `Watch.Add` accepts arbitrary C-syntax expressions. A fresh double-click at the apparent blank area did not reach that input row, so the identical coordinate action must not be repeated.
- The installed Ozone manual is readable after forcing UTF-8 output. Pages containing `Watched Data` include 176-178, 195, 197, 205, 298, and 311; those pages are the next source for the exact entry/shortcut workflow.
- Ozone manual page 176 lists four supported add paths: context-menu Watch, `Window.Add`, context-menu `Watch...`, or the last table input row. Page 316 documents the GUI shortcut as `Alt+Plus` and the script prototype as `Window.Add(const char* sWindow, const char* sSymbol)`.
- Before the configuration request, the diagnostic mailbox initializes `config_valid=0` and `config_firmware_type=255`; both new ELF fields resolve correctly in Ozone.
- Additional diagnostic fields resolve in the loaded ELF with reset values: `config_total_bytes=0`, `config_parameter_count=0`, and `config_motor_type=0`.
- Hardware interaction is presently blocked at the Windows-control layer: the sole Ozone window is minimized, and two independent activation attempts detected external user input. No read-only motor configuration request has been issued, so there is still no evidence-backed microstep/PPR correction and production `pulses_per_revolution` remains unchanged.

# Findings: ZDT X42S Serial Stepper Motor Driver

## Requirements
- Read the supplied ZDT X42S second-generation closed-loop stepper motor V1.0.5 manual.
- Create serial-control communication code in `bujin_motor`.
- Use the existing LibXR STM32 driver layer where compatible.
- Receive MaxicanPro `$BALL,valid,x_cm,vx_pixel_s,confidence,timestamp*` frames on STM32 and coordinate `bu_task` motor motion from valid messages.

## Research Findings
- `bujin_motor` is empty and can own the new motor class without displacing an existing driver.
- `LibXR::STM32UART` uses interrupt/DMA-backed `LibXR::UART` read/write ports. The class should depend on `LibXR::UART`, not raw STM32 HAL handles, so it remains aligned with the existing driver abstraction.
- The available browser cannot access the supplied `file://` PDF. A local `pypdf` parser was installed in the user temp directory for read-only extraction.
- The supplied manual is 110 pages and has extractable text. The serial TTL/RS485 frame definition is on manual pages 40-41, with the free-protocol command set beginning on page 46 and checksum details on page 109.
- The X42S supports both Emm5.0 and X firmware. Emm is the factory default; the manual explicitly identifies partial command differences, so one class must not silently issue a firmware-specific command under an ambiguous API.
- The communication-port function must be configured as `UART_FUN`; the motor supports TTL, RS232, and RS485 physical layers with the same free-protocol command section.
- User-prioritized behavior: set the current mechanical position to zero, then accurately reach a requested angle from an example thread under `bu_task`.
- Serial frame format is `[address, function, payload..., checksum]`. The motor default checksum is fixed `0x6B`; address `0` broadcasts and normal motor addresses are `1..255`.
- A positive acknowledgement is `[address, function, 0x02, checksum]`; `0x12`/`0x22` report an already-triggered homing/limit condition, `0xE2` is a parameter/state error, and `0xEE` is a frame-format error. Commands requesting a reach notification later emit `[address, function, 0x9F, checksum]`.
- `SetCurrentPositionAsZero` is the documented command `[address, 0x0A, 0x6D, checksum]` (manual page 48).
- Motor enable is `[address, 0xF3, 0xAB, enabled, sync, checksum]`; enabled=1 locks/enables and sync=0 executes immediately.
- Emm precise position command is `[address, 0xFD, direction, speed_rpm_be16, acceleration, pulses_be32, mode, sync, checksum]` (manual page 57). Mode `1` means absolute position relative to the coordinate zero; mode `0` is relative to the previous target and mode `2` is relative to the current live position.
- The manual's default 1.8-degree motor at 16 microsteps has 3200 pulses/revolution (0.1125 degree/pulse). The class should make pulses-per-revolution configurable and convert requested degrees with rounding; its absolute move method should use mode `1` to avoid cumulative relative-position error.
- The motor's default `Response=Receive` returns only the acceptance reply. `Response=Both` additionally emits `[address, 0xFD, 0x9F, checksum]` after entering the configured arrival window (default 0.8 degrees), and is supported by the class as an optional wait mode. The sample instead polls `ReadRealtimeAngle`, so it operates with the factory default and validates a user-configured angle tolerance.
- `ReadRealtimeAngle` is `[address, 0x36, checksum]` and returns `[address, 0x36, sign, position_be32, checksum]`. For Emm firmware the raw position is a single-turn angle: degrees = raw * 360 / 65536. The sample target is constrained to one turn for this post-move measurement.
- `LibXR::UART` can perform sequential thread-blocking writes and reads via `WriteOperation(Semaphore, timeout)` and `ReadOperation(Semaphore, timeout)`. The driver still transfers data with DMA; the task thread only blocks waiting for its completion/response.
- The STM32CubeMX project configures USART3 as 115200, 8N1, full duplex with DMA, matching the manual's default baud rate. It uses PD8 as TX and PD9 as RX.
- The root `task_plan.md` demonstrates the repository's expected discovery, implementation, build, and review workflow, but describes a completed LCD task. This task continues in the existing isolated motor plan.
- `maxican` does not exist yet and will be created as the dedicated MaxicanPro receiver module.
- USART1 is available for MaxicanPro: 115200 baud, 8N1, DMA RX/TX, PA9 TX, and PA10 RX. USART3 remains allocated to the stepper motor.
- `LibXR::Semaphore` is a counting FreeRTOS semaphore and `LibXR::Mutex` provides a blocking lock with an RAII `LockGuard`; together they support a latest-value mailbox that wakes the consumer without polling the UART.
- The motor manual defines immediate stop as `[address, 0xFE, 0x98, 0x00, checksum]`, with an acknowledgement response.
- The MaxicanPro line protocol has a start delimiter `$BALL,`, six comma-separated fields (`valid`, `x_cm`, `vx_pixel_s`, `confidence`, `frame_time_ms`), a `*` terminator, and newline. The parser will tolerate `CRLF`, reject overlong/malformed/non-finite fields, and bound confidence to `[0, 1]`.
- The receiver will publish both `valid=1` and `valid=0` measurements. Publishing invalid data wakes the motor task promptly so it can stop rather than continuing a previous target.
- The control command is configurable as `clamp(angle_offset + position_gain * (x_cm - center_cm) + velocity_gain * vx_pixel_s, min_angle, max_angle)`. The direction is determined by the configured gain sign.
- An individual motor-position command is sent with `wait_for_reached=false`, allowing new Emm position commands to update the target promptly. The existing motor class waits only for the normal command acknowledgement.
- `cmake --build --preset Debug` succeeds with the Maxican receiver and ball-tracking task linked into the STM32 firmware. The resulting image uses 48,496 bytes RAM (37.00%) and 106,164 bytes flash (10.12%).
- `BallMailbox` coalesces multiple updates before each motor-task wakeup, so a high frame rate produces a fresh target rather than an accumulated backlog of motion commands.
- `BallReceiver::ReadByte` supplies a dedicated `LibXR::ReadOperation` semaphore with an infinite timeout. LibXR's STM32 UART driver feeds DMA-received bytes into `ReadPort` from its receive-to-idle callback, so the receiver blocks between bytes instead of polling.
- On resume, `git diff --check` reports a blank line at EOF in `Core/Src/freertos.c`. That generated FreeRTOS file is outside the motor and Maxican module ownership, so it needs attribution before changing it.
- On resume, `User/app_main.cpp` constructs a Maxican receiver and starts its receive thread, but has the motor construction commented out and does not start `BuTask::BallTrackingThread`; therefore received ball data cannot currently move the motor.
- The resumed Debug build shows `zdt_x42s.hpp` is not compatible with `zdt_x42s.cpp`: it declares a much broader class with different command names and helpers, while the `.cpp` defines only the focused enable/zero/absolute-move/read-angle implementation. The header must be made authoritative for the implemented serial-control API before the firmware can compile.
- No application code depends on the unimplemented ZDT X42S declarations: production callers need only `Enable`, `StopImmediately`, `SetCurrentPositionAsZero`, `MoveToAbsoluteAngle`, `ReadRealtimeAngle`, and `IsAngleWithinTolerance`. The header can be safely narrowed to that implemented API.
- Final resume verification passed the Debug build and scoped whitespace checks. A full-repository whitespace check remains blocked only by `Core/Src/freertos.c:129`, an unrelated existing trailing blank line that was not modified as part of this task.

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Use a task-local planning directory | Root tracking files belong to a completed LCD integration task. |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| Planning init script produced no task files | Initialized this directory manually. |
| Browser security policy rejects the local file URL | Switched to a local parser rather than bypassing the restriction. |
| Python command-line Unicode path conversion failed | Use an ASCII suffix search rooted in the known local directory. |

## Resources
- `C:\Users\24137\xwechat_files\wxid_lr4ikrpn2i0q12_ee58\msg\file\2026-07\ZDT_X42S第二代闭环步进电机用户手册V1.0.5_260527(1).pdf`
- `bujin_motor`
- `Middlewares/Third_Party/LibXR/driver/st`

# Findings

External documents and generated text are recorded as reference data only.

## Initial context
- User reports that a target and an actual value exist, and they differ, but the stepper motor cannot be controlled.
- Required deliverables include a diagnosis/fix, automated Ozone debugging, and LCD display of ball state.
- Current reproduction detail: in task modes T3 through T6, the operator sees `R=+5.00` and `X=+12.5` while the stepper still does not respond.
- The stopping criterion for this continuation is live target validation that the control path actually drives the ball toward the requested setpoint; a successful UART acknowledgement by itself is not proof of physical ball motion.

## Vision and competition integration
- The available vision source is `E:\jie\main.py`. It sends a 13-byte `AA 55` UART frame at 115200 baud: valid byte, signed position in cm*100 (LE `int16`), signed pixel velocity*10 (LE `int16`), confidence*100, timestamp (LE `uint32`), then XOR checksum.
- Its calibrated ball axis maps pixel x=21 to -12.5 cm and x=460 to +12.5 cm. The STM32 `maxican` parser matches this format, including range/checksum validation and stale-frame timing.
- The current LCD already reports ball position (`X`), reference position (`R`), vision validity/confidence/age, controller target angle (`T`), and motor actual/goal angle (`M`/`G`). This directly presents the requested ball state without placing slow LCD traffic inside the UART/control threads.

## Root cause: X42S configuration gate
- `ZdtX42s::ReadMotorConfig()` deliberately returns `LibXR::ErrorCode::NOT_SUPPORT` when it receives a valid 37-byte X-firmware configuration response. It drains and validates this response because X firmware does not offer the EMM configuration payload.
- `BalanceController::StartTask()` and `StartViewMotorTest()` had been treating every non-`OK` configuration result as fatal. An X42S therefore stopped with `Configuration` before calling `Enable`, `SetCurrentPositionAsZero`, or `MoveToAbsoluteAngle`, even when valid vision data showed nonzero ball error.
- Transport failures (timeout, checksum and address errors) remain fatal. The firmware now treats only `NOT_SUPPORT` as a compatible, checked configuration result; it does not claim a readable EMM configuration, and it does not retry that harmless request every second.

## Root cause: telemetry frozen outside VIEW
- `BalanceController::Run()` executed `UpdateMonitorStatus()` only when the selected task was `Monitor`/VIEW and not running. If the user selected T3, T4, T5 or T6 but had not pressed RUN, the thread took an idle `Sleep()` branch and never drained `BallMailbox` or published a fresh status.
- As a result the LCD appeared to update only in VIEW even though the vision UART continued to send valid frames. The same branch prevented the operator from checking camera confidence and freshness before launching a selected task.
- The correction refreshes vision/LCD status in all non-running modes. The diagnostic motor-mailbox stays VIEW-only. In READY, the LCD reference column previews Task 3's +5 cm and Task 6's configured target; no motor command is sent until RUN is requested. COMPLETED/FAULT state retains its final elapsed/target values while its vision fields continue updating.

## Follow-up: task motion is still absent
- The controller calls `UpdateControl()` only while RUNNING and after a fresh `BallMailbox` update.  It then aborts on a stale/invalid/out-of-range frame before sending a motor command.
- `StartTask()` first sends motor enable and, by default, `SetCurrentPositionAsZero()`.  A failure at either stage stops the task before the first position command.  Existing Ozone mailbox data records standalone debug operations but not the task controller's start/control state, so a live controller snapshot is needed to distinguish those cases on the target.
- First Ozone T3 run with the new diagnostic mailbox reached `task_selected=3`, `task_run_state=Fault`, and `task_fault=MotorCommand` in target RAM.  It therefore did not stop due to missing vision or an omitted RUN request.  The exact failing request remains to be isolated between enable, set-zero and the first absolute-position command.
- A second Ozone T3 image skipped only `SetCurrentPositionAsZero()` while retaining motor enable, fresh-vision checks and the normal bounded `FD` command.  It produced the same `Fault/MotorCommand` result.  The issue is therefore not visual data, task selection, RUN handling, controller gains, or the optional task-start zero write; it is in the X42S command/acknowledgement path (enable or absolute position command).

## Confirmed X/Emm position-frame mismatch
- The motor has already returned a valid 37-byte `42 6C` X-firmware configuration reply. The V1.0.5 manual specifies that X firmware needs the 16-byte `FD` position frame: direction, two 16-bit ramp rates, 16-bit speed in 0.1 RPM, 32-bit absolute position in 0.1 degree, mode, sync and checksum. The existing 13-byte Emm pulse frame is not accepted as an X motion command.
- The operator observed `T=0` on LCD row 5 in T3/T4/T5 while the ball and motor values differ. This matches the recorded `MotorCommand` failure: the controller publishes a trial T then aborts after `FD` fails, returning the status target to zero. VIEW/PA5 runs a separate fixed-angle test path and therefore is not evidence that a closed-loop task command succeeded.
- The driver now persists the verified firmware family, encodes X `FD` frames and converts X command-36 position values from 0.1 degree units. The normal firmware rebuilt successfully after this change; Ozone task tests for T3/T4/T5 remain to be run.

## Required live proof after protocol correction
- An `FD` acknowledgement demonstrates only that the driver accepted a frame. It is not evidence that the shaft turned or that the ball moved.
- The target-side proof must correlate at least three values during a running task: `control_last_command_angle_degrees`, encoder feedback from command `36` (`task_motor_position_degrees` needs to be exposed if absent), and camera `task_position_cm` relative to `task_reference_cm`.
- Current application tuning is more conservative than the controller defaults: position gain `1.0 degree/cm`, velocity gain `0.002 degree/(pixel/s)`, clamp `+/-5 degrees`, minimum command delta `0.15 degree`, speed `120 RPM`, and acceleration parameter `240`.
- `UpdateMotorPosition()` already polls command `36` every 500 ms during RUN, but its position/validity and the read result are not copied into the Ozone mailbox. Adding telemetry-only fields is necessary to distinguish "accepted but no shaft motion" from "shaft moved but ball/control direction is wrong."
- The `OZONE_AUTOMATED_RUN_TASK` source path currently uses the ordinary task-start zeroing despite a stale CMake help string claiming it skips the zero write; validation must follow the source behavior.
- First live T3 halt from the corrected automated selector confirms `g_motor_debug_mailbox.task_selected = 3`; the harness is now definitely exercising T3 rather than the prior accidental VIEW path.
- The same approximately 1.5-second T3 snapshot reports `task_run_state = 3`, which maps to `Fault` in the current enum. The failure occurs well before the five-second T3 timeout, so the next value to read is `task_fault`/`task_start_stage`, not ball settling.
- `task_fault = 4` was observed in the same snapshot. Its semantic name must be taken from the source enum together with `task_start_stage`; the raw value alone is not sufficient.
- Follow-up `task_start_stage = 227 (0xE3)`, whose source marker is **T3 time limit elapsed**. This corrects the preliminary interpretation of fault value 4: the enum mapping must be re-read, because the definitive stage marker says the run survived startup/motion commanding until the T3 deadline rather than failing an `FD` transaction.
- Source enum verification: run state 3 is `Fault`, fault 4 is `TaskTimeout`, and stage `0xE3` is the matching T3-deadline branch. This test did not fail in enable, zeroing, or `FD` acknowledgement.
- At the deadline snapshot, `control_command_count = 1` and `control_last_command_angle_degrees = -5.0`, the configured negative saturation. The controller sent one accepted setpoint and did not resend because the requested target remained within the 0.15-degree delta. The next question is whether command-36 encoder feedback followed -5 degrees and whether camera X changed.
- Encoder evidence is now decisive: `task_motor_position_valid = 1` and `task_motor_position_degrees = +5.00976563` while the application command was `-5.0`. The shaft therefore moved about five degrees; the remaining failure is not "no UART/motor motion." The driver has an application-coordinate sign mismatch: motion commands apply `config_.positive_direction`, but `ReadRealtimeAngle()` returns the raw motor sign without transforming it into the same configured coordinate system.
- Deadline camera state is `task_position_cm = -11.0300005` with `task_reference_cm = +5.0`. Starting from the user's observed `X=+12.5`, the -5-degree command drove the ball across almost the full rod and never reversed, so the control loop has the feedback signs inverted: for this mechanism, both proportional position feedback and velocity damping must oppose measured error/motion. The current positive gains reinforce the direction after overshoot.
- First corrected negative-feedback T3 hold run moved the ball from approximately `-11.13 cm` to `+2.059 cm` while `R=+5.0`. The correction is physically effective, but the requested stop condition has not yet been reached; continue the same run window or tune bounded authority if it stalls.
- Continuing the same debugger session later showed `X=-10.05 cm`. Because the test may already have exceeded its 30 s extended deadline during debugger interaction, this cannot yet be classified as active closed-loop oscillation; read run state/stage before changing gains again.
- State verification confirmed `task_run_state=3` and `task_start_stage=0xE3`: the later drift happened after the 30 s T3 hold test timed out. It is not evidence that the corrected controller was actively driving in the wrong direction. A fresh reset with a strictly bounded sample interval is required.
- Fresh reset/run/halt sampling removed the timeout ambiguity. At 6 s and again after another 8 s of active T3 runtime, the controller remained `Running`, `R=+5.0`, while camera X stayed at approximately `-11.04 cm`.
- The same halt showed `control_command_count=1`, accepted command `+5.0 degrees`, and X42S encoder feedback `+4.3908 degrees`. UART framing, motor acknowledgement, and physical shaft positioning are therefore working; the ball remained against the negative end stop under a positive logical tilt.
- This direct sign test supersedes the earlier inference from the timeout snapshots: on this assembly positive logical angle drives X toward negative, so the `(measured-target)` P/D gains must be positive. The negative-gain test held the wrong tilt at the end stop. Acquisition authority is being raised from 5 to 10 degrees, within the documented 1..20 degree tuning range, to overcome end-stop static friction.

## Confirmed `0xE2` cause and endpoint recovery
- The latest `0xE2` was not a generic camera loss. New latched telemetry recorded reason bit `4` (absolute position outside the configured range), rejected X `+12.6100006 cm`, rejection duration `203 ms`, and zero accepted samples.
- The calibrated rod end is +12.5 cm, but the detected ball centre at the physical stop can read a few millimetres beyond the marker. A narrow application limit of +/-13.0 cm accepts this acquisition condition without relaxing the +/-20-degree motor limit.
- With +/-13.0 cm acceptance, the same T3 image remained Running for 594 samples and moved X from +12.6099996 toward +11.5900002 cm. Control sign and X-motor protocol are now confirmed correct; convergence speed/tuning and full T3-T6 acceptance remain unfinished.
# PDF task-target verification (2026-08-01)

- The user-provided official brief `H题_车载平衡滚球运动控制系统.pdf` defines the **ball-position** targets, not a fixed motor-angle `T` value: T3 is `+5 cm` from O then reverse and hold `-5 cm` in `<= 5 s`, with absolute position error `<= 1 cm`; T4 holds O (`0 cm`) while A→B in `<= 8 s`, error `<= 1 cm`; T5 holds O (`0 cm`) for one lap in `<= 30 s`, error `<= 1 cm`.
- Current firmware matches those task targets: `BalanceControllerConfig` defaults to `task3_positive_cm=5.0F`, `task3_negative_cm=-5.0F`, `position_tolerance_cm=1.0F`; `StartTask` selects `+5` then `UpdateTaskTarget` reverses to `-5`; T4/T5 set `target_position_cm=0.0F`; time limits are 5000/8000/30000 ms.
- The LCD fifth-row `T` is `target_angle_degrees`, a dynamic motor tilt command computed from camera position error, not the competition's ball-position target. At O in T4/T5, a correct controller often drives `T` toward 0. The decisive scoring target is the third-row X/R position (`R` is the reference in cm), not a fixed `T=±5` degree display.

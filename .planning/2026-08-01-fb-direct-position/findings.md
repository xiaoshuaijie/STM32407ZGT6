# Findings

- The active planning pointer belongs to an unfinished per-task PID change, so
  this task uses a separate plan directory and does not change `.active_plan`.
- X42S manual v1.0.5 section 5.3.8 defines FB as a 12-byte X-firmware direct
  speed-limited position command: address, FB, direction, speed in 0.1 RPM,
  position in 0.1 degree, motion mode, sync flag, checksum.
- Dynamic balance currently computes a new target every 20 ms and calls the FD
  API whenever the target changes by at least the configured command delta.
- The working tree already contains substantial user changes, including five
  task/stage PID profiles. The FB change must be layered onto those structures.
- The driver already detects X versus Emm firmware through `ReadMotorConfig`,
  encodes X-firmware FD separately, and centralizes command acknowledgements.
  FB can reuse the X angle/direction encoding and transaction helper.
- No repository-native unit-test target was found; validation will use focused
  source inspection, Debug firmware build, and `git diff --check`.
- The API will be named `MoveToAbsoluteAngleDirect`. It will reject non-X
  firmware with `NOT_SUPPORT`, use absolute mode/immediate execution, and may
  optionally wait for the FB `9F` reached frame for future discrete callers.
- Only the valid-measurement PID output path changes to FB. The vision-loss
  neutral command remains FD because it is a safety recovery action, not the
  continuously refreshed balance setpoint.
- `StartTask()` always calls `ReadMotorConfig()` before the first formal-task
  movement unless configuration was already checked. A valid X reply sets the
  driver's firmware family before `NOT_SUPPORT` is accepted by the controller,
  so the X-only FB API is available when the PID loop starts.
- Static call-site inspection confirms only the formal PID command at
  `UpdateControl` uses the direct API. VIEW phases, invalid-vision neutral,
  motor debug, Ozone relevel/recovery, and legacy task code still use FD.
- The implemented FB frame matches the manual offsets: command `0xFB`, speed
  at bytes 4-5 in 0.1 RPM, position at bytes 6-9 in 0.1 degree, absolute mode,
  immediate execution, then checksum. `git diff --check` passes.
- Debug firmware builds successfully after recompiling the driver, controller,
  application entry point, and dependent modules. Final image usage is FLASH
  134904 B (12.87%) and RAM 50984 B (38.90%). No compiler warnings were emitted.
- Final call-site review on 2026-08-02 confirms the formal control loop has the
  sole FB caller and the generated ELF remains present at `build/Debug/26diansai.elf`.
- The repository contains many unrelated existing modifications; none were
  reverted or folded into this task.

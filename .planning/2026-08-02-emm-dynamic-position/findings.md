# Findings

- The target reports `control_last_command_result = -7`, which is `LibXR::ErrorCode::NOT_SUPPORT`.
- `BalanceController::UpdateControl()` unconditionally calls `MoveToAbsoluteAngleDirect()`.
- `MoveToAbsoluteAngleDirect()` intentionally accepts only `FirmwareFamily::X` and returns `NOT_SUPPORT` for Emm.
- A successful 33-byte Emm configuration response sets `firmware_family_` to `Emm`; a 37-byte X response sets it to `X`.
- The existing `MoveToAbsoluteAngle()` already emits the correct firmware-specific `FD` position frame and accepts the controller acceleration setting.
- The compatible boundary is therefore X -> `FB` direct position, Emm -> `FD` absolute position.
- Existing worktree changes are user-owned and must be preserved.

# Progress: PC13 Zero Control

## 2026-08-01
- Loaded the PDF and planning workflow instructions.
- Confirmed the supplied 110-page ZDT_X42S V1.0.5 manual exists locally.
- Inspected the input/UI path, application wiring, GPIO generation, and current motor API.
- Distinguished coordinate clearing (`0A 6D`) from the requested homing-zero commands (`93 88`, `9A`).
- Added the LCD row-five target-angle requirement to the active scope.
- Preserved existing working-tree changes in the motor configuration readback path.
- Phase 1 discovery was completed after the full visual/text review of the relevant manual pages.
- Completed text and visual review of manual pages 61-65; confirmed persistent `93 88 01`, nearest-origin `9A 00 00`, and the `02/12/9F` response/status semantics.
- Resolved the bundled Poppler wrapper/path and console encoding issues by using direct Poppler executables and an ASCII temporary PDF copy.
- Added PC13 CubeMX/input wiring, a 1 s long-press detector with a separate short-click release event, and controller-thread request serialization.
- Added `SetSingleTurnHomingZero(true)` (`93 88 01`) and `TriggerHoming(0, false)` (`9A 00 00`) to the ZDT driver; long-press release is explicitly prevented from issuing homing.
- Updated LCD row five to show `F:<fault> T<target-angle>` while retaining the existing motor actual/target row.
- `cmake --build --preset Debug` passed after the implementation and after the long-press release fix. Final image: 50,832 B RAM (38.78%), 130,624 B flash (12.46%).
- `git diff --check` passed for all files touched by this task; only line-ending conversion warnings were emitted.
- Physical PC13 polarity, motor response, and homing motion remain unverified because no target board session is connected.

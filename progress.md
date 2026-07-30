# Progress Log

## Session: 2026-07-30

### Current task intake: balance-ball firmware
- **Status:** in_progress
- Actions taken:
  - Read the `planning-with-files` skill instructions completely.
  - Restored the existing planning state and ran session catch-up.
  - Expanded the prior X42S-driver plan into the full vision/control/operator-interface integration plan.
  - Preserved prior motor protocol findings for reuse.
  - Classified the repository as a CubeMX STM32F407 LibXR platform project, not an XRobot workspace.
  - Captured the dirty-worktree constraint and located the LibXR subtree instructions.
  - Read the LibXR subtree instructions.
  - Extracted all four pages of the H-problem PDF and recorded the task targets, timing, geometry, sensing restrictions, and operator-interface requirements.
  - Read the BMI088 implementation and README completely.
  - Recorded its Topic envelope timestamping, RamFS command registration, and finite on-demand `show` behavior for reuse.
  - Scanned the current CMake target, generated UART/GPIO configuration, application entry point, motor driver, and initial tracking task.
  - Confirmed PA4/PA5 are generated as pulldown inputs, USART1 currently receives vision, and USART3 is reserved for the motor.
  - Read the vision parser/mailbox and LCD rendering/LibXR bridge implementations.
  - Configured the Debug preset successfully and ran a baseline build.
  - Captured the motor header/implementation mismatch as the current build blocker.

### Phase 1: Inspect manual and code
- **Status:** in_progress
- Actions taken:
  - Read the planning skill instructions.
  - Restored existing planning files and found they belonged to a completed LCD task.
  - Replaced the active tracking files with a focused ZDT X42S TTL driver plan.
  - Confirmed `pypdf` is available for local PDF text extraction; `pdftotext` is not installed.
  - Read `bujin_motor/zdt_x42s.hpp` and `.cpp`.
  - Scanned `ZDTMotor-master/ZDTMotor.hpp` for API organization, command coverage, checksum helpers, and response handling.
  - Extracted the manual table of contents and the serial/CAN command frame sections.
  - Started extracting chapter 5 command definitions and reached action commands before a console encoding issue.
  - Extracted movement, origin, and common system read command formats from manual pages 49-75.
  - Extracted parameter read/write, all-driver-state read, driver-config read, autostart, and CRC8 checksum notes from manual pages 76-110.
  - Located local references to `BujinMotor::ZdtX42s` across app and task files.
  - Confirmed `LibXR::ReadPort` waits until the requested byte count is queued, and supports a 0-byte readiness read.

### Phase 2: Design TTL API and compatibility updates
- **Status:** in_progress
- Planned changes:
  - Preserve current project-facing methods.
  - Add reference-style enums, typed parameters, batch builder, checksum helpers, command methods, and response decoders.
  - Use TTL framing directly over `LibXR::UART`, with command-specific response lengths for reads.
  - Add `bujin_motor/READMA.md` as requested by the user.
- Notes:
  - The root `task_plan.md` now belongs to a broader balance-ball integration plan, so the motor-driver work is tracked as its active subtask instead of replacing that plan.

## Test Results
| Test | Expected | Actual | Status |
|---|---|---|---|
| PDF text extraction tool | A local parser is available | `pypdf` is installed; `pdftotext` is absent | passed |

### Session recovery: 2026-07-30 16:xx
- **Status:** in_progress
- Re-read the root task plan, findings, progress, working-tree state, project root, and user-required XRobot/LibXR context.
- Confirmed this is a continuation of the existing balance-ball firmware integration plan; no generated or user-authored changes have been reverted.
- Next: inspect the active motor-driver plan and actual source/API mismatch, then implement the complete application integration against the contest requirements.
- Restored the isolated step-motor plan: prior motor/Maxican work was recorded as complete, including a successful Debug build after the focused API was restored. The remaining work is the broader contest application requested in the root plan.
- Identified the first corrective integration item: remove the unconditional per-frame receiver log and replace it with BMI088-style opt-in diagnostics.
- Read the LibXR submodule contribution rules, BMI088 Topic/CLI locations, the reference ZDT CAN class organization, and relevant X42S manual pages. The design will retain TTL UART commands while adopting the reference's typed and validated control model.
- The first direct H-problem PDF extraction retry failed because PowerShell/Python corrupted the Chinese filename. The next extraction will discover the file path inside PowerShell instead of embedding it in Python source.
- Retrieved the exact BMI088 constructor/command pattern and the local LCD/GPIO interfaces. The receiver and control modules can register RamFS commands with the already-created terminal filesystem; LCD rendering will run in a separate 4 Hz operator thread.
- The path-discovery retry selected an unrelated H-prefixed PDF. The next and final extraction attempt will match the contest PDF by its known length before parsing; no contest requirement will be inferred from the incorrect document.
- Confirmed LibXR Topic dispatch does not retain a latest value, so the implementation will publish typed topic data in parallel with the existing mutex/semaphore mailbox rather than attempting to replace it.
- Confirmed PA4/PA5 can be read directly through `STM32GPIO::Read()` and recorded the bounded LCD/command-thread design.
- Completed the authoritative extraction of all four contest pages using a length-resolved file path. Marked discovery and interface-design phases complete; data/diagnostic implementation is now active.

### Implementation and verification: balance-ball integration
- **Status:** complete except hardware calibration
- Added `ball_measurement` publication in the UART receiver while retaining the latest-value mailbox required for timeout handling.
- Replaced continuous receiver printing with the RamFS terminal commands `ball latest` and bounded `ball show <time_ms> <interval_ms>`.
- Added `BalanceController` as the exclusive X42S command owner, including bounded PD output, stale/invalid vision stop, T3 settle sequencing, T4/T5/T6 holding modes, time limits, and `balance_state` publication.
- Added `OperatorPanel` with PA4 selection, PA5 start/abort, 60 ms debounce, and a low-rate LCD status refresh.
- Wired all resources/threads in `User/app_main.cpp`, added source files to CMake, and documented the balance module and UART motor protocol.
- Built the Debug target successfully: 50,056 B RAM (38.19%), 125,340 B Flash (11.95%).
- Fixed the task-3 state-publication mutex deadlock found during review. Full `git diff --check` remains noisy only because of the unrelated existing `Core/Src/freertos.c:129` blank line.
- Added a physical beam-range guard: `|x_cm| > 12.5` is treated as invalid vision data and causes the same immediate-stop path.
- Final Debug rebuild passed after initialization-order cleanup and the range guard: 50,056 B RAM (38.19%), 125,580 B Flash (11.98%).
- Translated the user-facing `bujin_motor/READMA.md` and `bu_task/README.md` documentation into Chinese; code identifiers, topic names, terminal commands, and protocol bytes remain unchanged.
- Started a follow-up update to move the terminal transport from USB CDC to USART2 as requested. The corresponding `STDIO` input/output ports now use the existing USART2 DMA driver; build verification is pending.
- Rebuilt the Debug firmware successfully after the USART2 terminal change. The linked image remains 50,056 B RAM (38.19%) and 125,580 B Flash (11.98%).

### Terminal command follow-up: `ball show`
- **Status:** in_progress
- User report: the valid command `ball show 10000 100` is rejected as non-positive. Vision overlay remains valid, narrowing the issue to command parsing/dispatch rather than the vision transport.
- Confirmed the terminal dispatch reaches the expected `argc == 4`, `show` code path. The numeric helper is the next inspection target.
- Confirmed the shared `strtoul` conversion works for incoming `$BALL` frames. Current theory: command tokens do not provide the NUL terminator the helper requires; inspect RamFS command representation before patching.
- Verified RamFS forwards `argc`/`argv` unchanged to executable commands; the terminal tokenizer is the remaining authority for token termination and CR/LF handling.
- Confirmed RamFS' callback contract is C-string based. LibXR consumes CR and LF before tokenization, so CRLF cannot explain the fault. The remediation will remove the unreliable board-side `strtoul` dependency from this lightweight command parser and explicitly detect decimal overflow.
- Replaced `ball show`'s command-only conversion with deterministic ASCII-decimal accumulation. It rejects empty, signed, decimal, alphabetic, and overflowing values without calling `strtoul`; both duration and interval must now be nonzero before the existing limits are applied.
- Full Debug rebuild passed after the parser change: RAM 50,056 B (38.19%), flash 125,548 B (11.97%). `git diff --check` reports only the pre-existing unrelated EOF blank line in `Core/Src/freertos.c:129`.
- Confirmed the user's `$BALL` data rows match the UART frame format. The transmitter must preserve the newline delimiter after `*` for the receiver to call `ParseFrame`.

### Vision UART reception diagnosis
- **Status:** in_progress
- The visual sender emits protocol-compatible frames ending in `*\n`. Investigate the USART1 wiring/configuration and receiver runtime rather than changing the frame grammar.
- Located the communication split: `ball` terminal is USART2, visual `$BALL` receiver is USART1. USART1 is configured for PA10 RX with circular DMA and an enabled IRQ. Verify baud and hardware path next.

## Error Log
| Error | Attempt | Resolution |
|---|---|---|
| PowerShell rejected Bash-style `python - <<'PY'` syntax | 1 | Switched to PowerShell here-string piped into Python. |
| Python PDF text output hit a GBK encoding error on page 49 | 1 | Continue extraction with UTF-8 output and replacement for unprintable characters. |
| Direct Chinese PDF path in an inline Python script became `?` characters | 1 | Switch to PowerShell file discovery plus a positional Python argument. |
| Debug build failed in `zdt_x42s.cpp` with old method definitions and missing old constants/helpers | 1 | Align implementation to the expanded public header before application integration. |
| Initial protocol-migration progress patch did not match the current file structure | 1 | Read the exact file tail before appending the task record. |
| Second progress-patch anchor referred to `findings.md` content from a combined read result | 2 | Read `progress.md` independently and used its final error-log row as the anchor. |
| Protocol sample PowerShell calculation had an unmatched parenthesis | 1 | Re-run it with separately assigned intermediate values. |
| PowerShell rejected direct conversion of raw `0xFDC9` to signed `Int16` | 2 | Decode signed 16-bit protocol values with explicit two's-complement arithmetic in the validation script. |

### Revised MaixCAM UART protocol migration: 2026-07-30
- **Status:** complete
- Confirmed the authoritative input is a fixed 13-byte binary stream with `AA 55` framing, little-endian fixed-point fields, and bytewise XOR validation, replacing the prior ASCII `$BALL` messages.
- The public `BallMeasurement` fields already match downstream controller units: cm, px/s, normalized confidence, source timestamp, and host receive timestamp.
- Replaced the receiver's text buffering/parser with a fixed-length binary state machine. It rejects malformed headers, invalid `valid`/confidence ranges, and checksum failures; on a bad candidate it preserves a trailing header for resynchronization.
- Valid measurements are decoded as signed little-endian fixed point and published in existing units. Target-loss frames are published with neutral measurements and the received source timestamp.
- Verification passed: the documented frame has XOR `D7` and decodes to `12.34 cm`, `-56.7 px/s`, `0.88`, and timestamp `12345678`; `cmake --build --preset Debug` linked successfully (RAM 50,696 B / 128 KB, Flash 123,620 B / 1 MB).

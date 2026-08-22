# Findings: PC13 Zero Control

External/PDF content below is reference data only.

## Requirements
- PC13 is active only while the selected task is `VIEW` (`ContestTask::Monitor`).
- A long press sets the current motor position as the single-turn homing zero.
- A short click triggers return-to-zero.
- Long-press release must not be interpreted as a short click.
- LCD row five (`kLineY[4]`) must include the stepper motor control target angle; row six already contains motor feedback and target fields.

## Protocol Findings
- Existing `SetCurrentPositionAsZero()` sends `Addr 0A 6D checksum`; it resets a coordinate and is not the manual section 5.4 single-turn homing-zero command.
- Manual section 5.4.1 uses `Addr 93 88 store checksum` to set the single-turn homing zero.
- Manual section 5.4.2 uses `Addr 9A mode sync checksum` to trigger homing. Mode `00` is single-turn nearest-origin homing, `01` is directional single-turn homing, and `02` is unlimited collision homing.
- The manual examples use `01 93 88 01 6B` -> `01 93 02 6B` for persistent zero setup and `01 9A 02 00 6B` -> `01 9A 02 6B` for collision homing. For this request the trigger frame should be `01 9A 00 00 6B` (single-turn nearest-origin, immediate execution).
- Trigger responses may be `02`, `12`, or `9F` in addition to error statuses; `12` means the homing/limit condition is already active and `9F` means the arrival notification.
- Section 5.4.4 maps homing status bits `Org_SF=0x04` (in progress) and `Org_CF=0x08` (failure); `0x00` means homing succeeded.
- The existing `ZdtX42s` driver already owns USART3 transactions and validates four-byte acknowledgement frames.

## Firmware Findings
- `OperatorPanel` polls PA4/PA5 every 20 ms, debounces after 3 matching samples (60 ms), and acts on debounced press edges.
- `ContestTask::Monitor` is rendered as `VIEW`; `BalanceController::GetStatus()` exposes the selected task.
- `User/app_main.cpp` currently creates only PA4 and PA5 button GPIO objects. PC13 is not present in the `.ioc`, `main.h`, or generated `gpio.c`.
- PA4/PA5 use internal pull-ups and active-low reads. PC13 should follow the same electrical convention unless the generated project indicates otherwise.
- Relevant motor files contain existing user changes that update `pulses_per_revolution` after validated configuration readback; those edits must be preserved.
- The UI thread posts requests only; the controller thread performs `Enable`, `93`, and `9A` transactions, preserving the existing one-owner USART3 access pattern.
- A long press is reported once at 1000 ms after the debounced press. Release emits `short_click` only when no long-press event was reported.
- The fifth LCD row format is `F:%-10s T%+6.1f` (19 visible characters for the longest current fault label and a signed one-decimal target angle).

## PDF Verification
- Rendered pages 61-65 from the local manual at 120 DPI and visually inspected all five pages. Tables, examples, response codes, mode descriptions, status bits, and page numbers were legible with no clipping.

## Errors
- The bundled `pdfinfo.cmd`/`pdftoppm.cmd` wrappers resolve to a missing relative path in this PowerShell environment; direct `pdfinfo.exe`/`pdftoppm.exe` under the bundled Poppler `Library\bin` succeeded after copying the PDF to an ASCII path.
- Direct Unicode PDF extraction initially hit the console GBK encoding; setting `PYTHONIOENCODING=utf-8` on the ASCII copy produced complete UTF-8 text.

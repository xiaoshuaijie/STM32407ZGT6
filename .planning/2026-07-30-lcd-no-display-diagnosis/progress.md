# Progress: LCD No-Display Diagnosis

## Session: 2026-07-30

### Phase 1: Trace display path
- **Status:** in_progress
- Actions taken:
  - Read the persistent-planning instructions and restored existing workspace context.
  - Located LCD source files and identified the STM32F407 CubeMX/CMake project structure.
  - Preserved the dirty worktree and began a read-only diagnosis.
  - Confirmed the LCD is initialized before worker threads start and that CMake includes all LCD bridge/source files.
  - Cross-checked SPI1 and LCD pin definitions across the application, CubeMX configuration, generated GPIO/SPI code, and `main.h`.
  - Identified a double RGB565 byte swap in `lcd_fill`; it affects colors but does not explain a black screen.
  - Confirmed the operator-panel worker has an immediate and periodic five-line text rendering path, and that LCD initialization runs after scheduler startup inside the default task.
  - Identified silent error handling on the LCD SPI bridge as an observability gap.
  - Found the primary code defect: PD7 is labeled `LCD_CS` but configured as a floating input, and the LCD driver never asserts chip select. This prevents the panel from receiving all commands and display data.
  - Confirmed the LibXR GPIO wrapper does not set GPIO mode on construction, so it cannot compensate for PD7's generated input configuration.
  - Ran `cmake --build --preset Debug`; Ninja reported no work to do and no build errors.
  - Confirmed the `.ioc` explicitly marks PD7 as `GPIO_Input` and that no code calls `lcd_io_cs()`.
  - Completed the diagnosis: chip select is the definite blank-screen cause; byte-order handling and panel/power compatibility are secondary follow-ups.
  - Confirmed every diagnosis-plan phase is complete. The final read emitted an unrelated Terminal-Icons profile XML warning; it did not affect the command or firmware analysis.
- Files created/modified:
  - `.planning/2026-07-30-lcd-no-display-diagnosis/task_plan.md`
  - `.planning/2026-07-30-lcd-no-display-diagnosis/findings.md`
  - `.planning/2026-07-30-lcd-no-display-diagnosis/progress.md`

## Test Results
| Test | Expected | Actual | Status |
|------|----------|--------|--------|
| Initial workspace scan | Locate LCD and build configuration | LCD source and CubeMX/CMake files located | passed |
| Debug build | Build current firmware target | No compile/link failure | Ninja completed with no work to do | passed |

## Error Log
| Error | Attempt | Resolution |
|-------|---------|------------|
| None | 0 | Not applicable. |
| Terminal-Icons profile XML warning during plan read | 1 | Ignored: the requested file was read successfully and the warning is outside the firmware workspace. |

### Phase 4: Implement chip-select fix
- **Status:** complete
- Actions taken:
  - Restored the completed diagnosis plan after the user authorized the fix.
  - Reconfirmed the current user-owned diff before editing: PD7 is presently a generated input and the LCD sources are already included in the target.
  - Changed PD7 to `GPIO_Output` in CubeMX configuration.
  - Configured PD7 as a high-idle push-pull output in generated GPIO initialization.
  - Asserted active-low CS before LCD reset/initialization commands.
  - Rebuilt and linked the Debug target successfully.
- Files modified:
  - `26diansai.ioc`
  - `Core/Src/gpio.c`
  - `LCD/lcd_core.c`

| Test | Expected | Actual | Status |
|------|----------|--------|--------|
| LCD CS fix build | Rebuild touched sources and link firmware | Successful Debug image | Rebuilt `gpio.c` and `lcd_core.c`; linked `26diansai.elf` | passed |

| Error | Attempt | Resolution |
|-------|---------|------------|
| First plan-status patch did not match the existing test-table format | 1 | Re-read the exact file tails and applied a narrower update; source files were unaffected. |

### Phase 5: Ozone online hardware diagnosis
- **Status:** in_progress
- Actions taken:
  - Read the computer-use and persistent-planning instructions.
  - Restored the LCD diagnosis plan and located the known-good 1.47-inch LCD template.
  - Preparing non-invasive Ozone checks for firmware execution, GPIO state, SPI state, and template comparison.
  - Initialized the Windows automation runtime and located the registered Ozone V3.40g application.
  - First launch by registered app ID exposed no targetable Ozone window; resolving the supplied shortcut target before retrying with an explicit executable path.
  - Resolved the shortcut target and launched Ozone successfully through Computer Use. The debugger is at the project-selection dialog, ready to load the firmware ELF with symbols.
  - Confirmed there is no existing Ozone project in this workspace; the known-good template provides a Keil AXF image and complete LCD source for comparison.
  - Located the current Debug ELF: `build/Debug/26diansai.elf`.
  - Compared the working template: it proves the board uses an active-low PD3 backlight/power control and a 1.47-inch panel profile. Current firmware has both settings reversed/mismatched; Ozone will now validate live target state before patching.
  - Opened Ozone's new-project target wizard. User input invalidated the current window, then recovery showed Ozone had exited. No debugger session attached to the MCU and no target state was changed; restart from a new Ozone window.
  - Added UART/Maxican verification for the user-supplied `$BALL` frames; inspect source and configuration before modifying the transport.
  - Verified USART1 PA10 / 115200-8N1 / DMA2 Stream2 reception configuration and Maxican's line-oriented parser. All five supplied payloads satisfy parser field/range/length requirements, conditional on a trailing LF or CRLF after `*`.
  - Confirmed the 1.47-inch template's LCD settings are the hardware-matching correction, and prepared the LCD backlight/panel/line-buffer and Maxican frame-termination updates for build verification.
  - Updated the LCD configuration to the working board profile (active-low PD3, 1.47-inch, rotate 90, 320-pixel line buffer) and changed Maxican parsing to complete at `*`; the Debug ELF linked successfully (RAM 50,696 B / 128 KiB, FLASH 125,596 B / 1 MiB).
  - Reopened Ozone and attempted its `Start Debug Session -> Download & Reset Program` action. The window still exposes `Start Debug Session`, so it did not establish a usable target session; no live MCU registers were read through Ozone yet.
  - Ozone reloaded the rebuilt ELF, and its unsaved debugger project was saved as `build/Debug/26diansai.jdebug` before exit.
  - Verified that Windows enumerates the connected J-Link EDU Mini V2 (USB serial `000802008886`). Direct SEGGER connection fails before target access because the probe firmware update times out; no STM32 flash programming or register reads occurred.
  - With explicit user authorization, attempted the J-Link EDU Mini V2 firmware update through SEGGER J-Link Configurator V9.36. USB detach and reattach completed, but firmware write/verify failed; firmware remains dated 2025-10-13 and Configurator is non-responsive. The STM32 ELF was not programmed.

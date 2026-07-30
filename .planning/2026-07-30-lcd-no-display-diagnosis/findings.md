# Findings: LCD No-Display Diagnosis

## Requirements
- Determine why the LCD shows nothing by inspecting the current code and project configuration.
- Implement the verified PD7/LCD_CS corrective action and rebuild the firmware.

## Initial Context
- Project: STM32F407 CubeMX/CMake firmware.
- Display source files: `LCD/lcd_core.c`, `LCD/lcd_port.c`, `LCD/lcd_font.c`, and `LCD/lcd_libxr_port.cpp`.
- Existing project records note that LCD drawing uses blocking SPI writes and must run at a low refresh rate.
- The repository is dirty and contains user-owned modifications; no existing source changes will be reverted.

## Research Findings
- `User/app_main.cpp` constructs a LibXR wrapper for SPI1 and four LCD GPIOs, builds an `lcd_io` object, and calls `lcd_init_dev(&lcd1, LCD_1_14_INCH, LCD_ROTATE_0)` before starting the balance UI thread.
- The LCD source is included in the CMake target, including the C-to-LibXR bridge `LCD/lcd_libxr_port.cpp`; this is not an omitted-source build failure.
- CubeMX maps SPI1 to PB3 (SCK) and PB5 (MOSI), LCD DC to PB4, and LCD power/reset/chip-select to PD3/PD4/PD7. `Core/Inc/main.h`, generated GPIO code, `.ioc`, and the `app_main.cpp` resource construction agree on those mappings.
- SPI1 is configured as 8-bit, master, one-line transmit, MSB first, software NSS, mode 3 (CPOL high, second edge), at an estimated 5.25 Mbit/s. This must match the actual LCD controller; a common ST7789 panel normally accepts mode 0 or mode 3, but the panel data sheet is the authority.
- All LCD GPIOs start low. The initialization drives reset low then high and drives `LCD_PWR` high through the `bl` field. The assumed active level of PD3 must match the physical module power/backlight circuit.
- `lcd_init_dev` immediately clears the display black. It does not print a visible test string; therefore a blank black panel after initialization cannot distinguish a working display from a display that receives no commands until the operator panel draws.
- Without a line buffer, `lcd_clear` sends one pixel at a time. This is inefficient but should not by itself produce a permanently blank panel.
- `lcd_fill` swaps the RGB565 color before calling `lcd_draw_point`, which swaps it a second time. Non-black colors are therefore transmitted byte-reversed during fill/clear paths; this is a color defect, not an explanation for an entirely blank display because black is unchanged.
- `BuTask::OperatorPanel::Run()` first sets a white-on-black font, clears the LCD, then calls `Render()` immediately and every 500 ms. `Render()` writes five ASCII text lines at y=0, 14, 28, 42, and 56, so there is a concrete visible draw path after initialization.
- `app_main()` is invoked by the FreeRTOS default task after HAL, GPIO, DMA, SPI1, and all UART initialization. LCD initialization runs from that task, so `lcd_delay()` using `LibXR::Thread::Sleep()` is valid at this point.
- The UI thread has a 1024-byte LibXR stack request. It uses `lcd_print`, whose local formatted buffer is 128 bytes. The project needs a stack-watermark/runtime check, but source alone does not show a definite overflow.
- `lcd_libxr_spi_transmit` returns the SPI error status, but `LCD/lcd_port.c` explicitly discards that return. A failed SPI transaction is silent and therefore can present exactly as a blank panel.
- **Primary code defect:** PD7 is `LCD_CS` in `Core/Inc/main.h` and the CubeMX `.ioc`, but `Core/Src/gpio.c` configures `LCD_CS_Pin` as `GPIO_MODE_INPUT` with `GPIO_NOPULL`. The application still wraps PD7 as `STM32GPIO LCD_CS`, but `lcd_init_hw` never calls `lcd_io_cs`; it remains high-impedance for the full initialization and rendering sequence. An SPI LCD requires CS asserted (normally driven low) to accept commands. With no external pull-down, this directly explains an entirely blank LCD despite correct SCK/MOSI traffic.
- The rest of the generated LCD pins are output push-pull. The CS line is the only LCD control line not configured as an output, so this is not an intentional common configuration pattern.
- Corrective configuration: make PD7 an output push-pull, no pull, initial high; drive it low before the initialization command sequence and keep it low for each command/data transfer (or tie it low only if the board's CS is intentionally permanently selected). The CubeMX `.ioc` must also be changed so regeneration preserves the setting.
- `STM32GPIO` construction does not configure a pin. It only stores the port/pin values. `STM32GPIO::Write()` merely writes the BSRR register and cannot change PD7 from input mode to an output. Therefore wrapping PD7 in `app_main.cpp` does not remedy the CubeMX configuration mistake.
- `STM32SPI::TransmitBlocking()` uses HAL blocking transfers and returns an error if the peripheral is not ready. The current Debug target is already successfully built, so this investigation found no compile/link cause for the blank display.
- The `.ioc` explicitly records `PD7.Signal=GPIO_Input`, proving the generated input configuration is intentional CubeMX state rather than an accidental C-source mismatch.
- `lcd_io_cs()` occurs only as a declaration and definition; there are no invocation sites anywhere in the project. The CS pin is never asserted during LCD initialization or drawing.

## Prioritized Outcome
1. **Definite cause:** PD7/LCD_CS is high-impedance and never asserted, so the LCD controller does not receive SPI traffic.
2. **Secondary code defect:** `lcd_fill` double-swaps non-black RGB565 colors. Correct after chip select is fixed; it cannot create a completely blank panel.
3. **Hardware/configuration checks after the definite fix:** verify PD3 produces the active backlight/power level, and verify the physical module is the expected 1.14-inch controller/rotation profile and accepts SPI mode 3. These cannot be proven from source alone.

## Implementation Plan
- Set `PD7.Signal=GPIO_Output` in the CubeMX `.ioc` configuration.
- Configure PD7 as an output push-pull pin with a high idle level in `MX_GPIO_Init`.
- Select the LCD by calling `lcd_io_cs(plcd->io, 0)` before its command sequence.
- Do not change the unrelated RGB565 byte-order defect in this focused fix.

## Fix Verification
- `PD7.Signal` is now `GPIO_Output` in the `.ioc` file.
- `MX_GPIO_Init` sets PD7 high before configuring it as output push-pull, no-pull, low-speed.
- `lcd_init_hw` now asserts active-low CS before reset and the first LCD command.
- `cmake --build --preset Debug` rebuilt the touched C sources and linked `26diansai.elf` successfully.

## Online Debug Context
- The user has connected a J-Link and requested Ozone 3.40g debugging through the supplied shortcut.
- The known-good comparison project is `C:\Users\24137\xwechat_files\wxid_lr4ikrpn2i0q12_ee58\msg\file\2026-07\STM32F407VGT6_Template\1.LCD_1.47_template` and contains `BSP`, `Components`, `Core`, `Drivers`, and `MDK-ARM` directories.
- Online checks will establish the program counter, LCD initialization activity, GPIO output data registers, and SPI1 state before drawing a conclusion.
- Ozone V3.40g is registered on this machine, but the first Computer Use launch by its registered app ID did not expose a window. Recover by resolving the user-supplied shortcut target and launching that explicit existing executable through Computer Use.
- The supplied shortcut resolves to `C:\Program Files\SEGGER\Ozone\Ozone.exe`. Ozone is now open at its project-selection dialog; no board state has yet been changed through the debugger.
- This workspace has no checked-in Ozone project. The comparison template contains a Keil `STM32F407VGT6.axf`; the current firmware's build artifact must be located outside the source-file search before opening an Ozone session with symbols.
- The current signed firmware image is `E:\stm32cubemx exe\26diansai\build\Debug\26diansai.elf`.
- **Template comparison, primary new evidence:** the working 1.47-inch template uses `LCD_1_47_INCH`, `LCD_ROTATE_90`, and `{LCD_PWR_GPIO_Port, LCD_PWR_Pin, 1}`. Its backlight line is active-low: it initializes PD3 low and `lcd_io_bl(..., 1)` preserves the low level because the descriptor inverts it. The current project uses `LCD_1_14_INCH`, `LCD_ROTATE_0`, and `{&LCD_PWR, false}`, so it drives PD3 high during `lcd_init_hw`, which is the opposite backlight state on this known-good board.
- The template keeps LCD CS low from GPIO initialization onward and uses the same SPI1 mode, but with a prescaler of 8 rather than 16. The speed difference is secondary; the template proves PD3 active-low backlight polarity and 1.47-inch panel type are required.
- The template supplies a 320-entry line buffer. The current project has none; this makes screen clearing dramatically slower but does not explain an unlit display.
- During Ozone setup, user input invalidated the captured debugger window. A recovery check found the Ozone process had exited, so no target connection or board mutation occurred. A fresh Ozone session is required; do not reuse the old wizard state.

## Added UART Check
- The user supplied five valid-looking `$BALL` payloads and requested verification that UART/Maxican can receive them.
- Inspect the active UART1 configuration, Maxican frame delimiter logic, and runtime startup path. In particular, determine whether the transmitted wire format carries a line ending after `*`.
- `BallReceiver` is constructed with `usart1`, while the terminal is intentionally on `usart2`. Vision sender TX must therefore connect to MCU **PA10 / USART1_RX**, not PA3 / USART2_RX; the UART2 terminal cannot prove that Maxican received a frame.
- USART1 is configured as **115200 baud, 8 data bits, no parity, 1 stop bit, no hardware flow control**. PA9/PA10 use AF7, RX uses DMA2 Stream2 in circular mode at very-high priority, and USART1 plus DMA2 Stream2 interrupts are enabled.
- The supplied five payloads are syntactically valid for `ParseFrame`: exactly five fields after `$BALL,`, valid values for `valid`, position, velocity, confidence, and unsigned source timestamp; each is far below the 96-byte frame limit.
- **Required wire format:** `BallReceiver::Run` only calls `ParseFrame` when it receives `\n`. Valid transmitter output is `$BALL,1,7.35,309.7,0.75,853411*\n` (or `*\r\n`). A stream ending only in `*` remains buffered and is never published.
- Once a line is accepted, the receiver stores it in `BallMailbox`, publishes the `ball_measurement` topic, and `ball latest` on USART2 prints it. The display/control path consumes the mailbox, not the terminal UART.

## Corrective Changes Pending Verification
- The known-good template establishes that this board uses a 1.47-inch LCD profile, `LCD_ROTATE_90`, and an active-low PD3 backlight/power signal. Update `app_main.cpp` to use the matching profile and backlight inversion, and attach a 320-pixel line buffer for reliable, fast screen clears.
- End Maxican frames at `*` rather than requiring a following line-feed. This preserves normal CRLF compatibility and accepts the supplied frame payloads even when the sender terminates precisely at `*`.

## Online Debug Result
- Windows recognizes the attached probe as `J-Link EDU Mini V2`, USB instance `USB\\VID_1366&PID_1020\\000802008886`.
- Neither Ozone nor J-Link Commander can establish a usable debug session. J-Link Commander reaches `Connecting to J-Link via USB...`, then reports an automatic firmware-replacement attempt from `J-Link EDU Mini V2 compiled Oct 15 2025 17:03:08` to `compiled Mar 26 2026 10:17:59` and fails with `Communication timed out - Can not execute firmware update` followed by `Cannot connect to the probe/programmer.`
- This is a J-Link probe/USB firmware-communication failure before the STM32 target is accessed. No target register was read and the rebuilt ELF was not programmed.
- User explicitly authorized a manual update in SEGGER J-Link Configurator V9.36. The configurator identified the probe as `J-Link EDU Mini V2.00`, selected it, and attempted the proposed `2026-03-26` firmware. Its updater successfully waited for the probe to detach (`OK after 109 ms`) and reattach (`OK after 117 ms`), then reported `ERROR: Firmware update failed`. The device list still shows the old `2025-10-13` firmware; Configurator became non-responsive after the failure.

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Trace the executable call chain before analyzing register-level details | A blank screen commonly results from omitted initialization or no draw call, which can be established directly from source. |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| Existing root planning documents describe previous work | This diagnosis uses an isolated plan directory to avoid overwriting that context. |
| LCD reset/backlight polarity cannot be proved from firmware alone | Keep it as a targeted board wiring/voltage check after code-path verification. |

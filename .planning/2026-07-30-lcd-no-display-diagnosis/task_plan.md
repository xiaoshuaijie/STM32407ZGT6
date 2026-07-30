# Task Plan: LCD No-Display Diagnosis

## Goal
Determine, from the STM32 firmware and CubeMX configuration, the code-level reasons the LCD may remain blank and report the evidence-backed corrective actions.

## Current Phase
Phase 5 - Ozone online hardware diagnosis

## Phases

### Phase 1: Trace display path
- [x] Inspect application construction, LCD initialization, and rendering calls.
- [x] Inspect LCD port pin mapping and SPI transport setup.
- [x] Compare the source mapping with CubeMX GPIO/SPI configuration.
- **Status:** complete

### Phase 2: Assess failure modes
- [x] Check initialization order, reset/backlight/DC/CS behavior, and controller commands.
- [x] Identify build or linkage omissions and runtime blockers.
- [x] Separate code evidence from hardware-only checks.
- **Status:** complete

### Phase 3: Deliver diagnosis
- [x] Verify findings against the built target/source call graph.
- [x] Report prioritized causes and precise checks/fixes.
- **Status:** complete

### Phase 4: Implement and verify chip-select fix
- [x] Change PD7/LCD_CS to a CubeMX output and update generated GPIO initialization.
- [x] Assert active-low chip select in the LCD initialization sequence.
- [x] Build the Debug target and inspect the final diff.
- **Status:** complete

### Phase 5: Ozone online hardware diagnosis
- [ ] Launch Ozone through the provided shortcut and attach to the connected J-Link target.
- [ ] Verify execution reaches LCD initialization and inspect PC/SP plus relevant GPIO/SPI registers.
- [ ] Compare live configuration and the working template's LCD port definitions.
- [ ] Trace USART1 and Maxican reception of the supplied `$BALL` frames.
- [ ] Report the evidence-backed cause and apply only verified corrective changes.
- **Status:** blocked (J-Link EDU Mini V2 probe firmware communication timeout prevents SWD connection)

## Key Questions
1. Does the application initialize and draw to the LCD after its SPI/GPIO dependencies are ready?
2. Do the LCD port definitions use the same physical pins and SPI peripheral configured by CubeMX?
3. Does the display controller initialization match the panel's controller, resolution, and wiring?

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Diagnose before changing firmware | The request asks why no display appears; unverified code changes could hide the actual board issue. |
| Treat PD7 chip-select configuration as the primary code defect | `PD7` is the named LCD CS pin but is initialized as high-impedance input, while the LCD driver never drives it. |
| Keep CS low after initialization | SPI1 has no other device in this application, so keeping the active-low LCD selected safely covers both initialization and all later rendering. |
| Start online diagnosis with non-invasive checks | Breakpoints and register reads establish firmware execution and pin states before any additional code change. |
| Validate UART framing before changing Maxican | The receiver parses a complete newline-delimited record, so line termination is part of the on-wire protocol. |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| None | 0 | Not applicable. |

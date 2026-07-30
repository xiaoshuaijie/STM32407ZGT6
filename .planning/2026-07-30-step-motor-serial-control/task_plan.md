# Task Plan: ZDT X42S Serial Stepper Motor Driver

## Goal
Implement a LibXR STM32-compatible C++ serial-control class in `bujin_motor`, a MaxicanPro `$BALL` UART receiver, and coordinated task behavior that drives the ZDT X42S from valid received ball-position data.

## Current Phase
Complete

## Phases

### Phase 1: Protocol and integration discovery
- [x] Extract and validate serial frames, checksums, responses, and units for zeroing and Emm position control from the manual.
- [x] Inspect `bujin_motor` and LibXR STM32 serial driver interfaces.
- [x] Record requirements and constraints.
- **Status:** complete

### Phase 2: API design
- [x] Define a focused, type-safe motor-control API matching the documented protocol.
- [x] Decide integration and build-file changes using existing repository conventions.
- **Status:** complete

### Phase 3: Implementation
- [x] Add the serial protocol encoder, parser, and command methods under `bujin_motor`.
- [x] Integrate the new sources into the firmware build.
- [x] Add the `bu_task` example thread that zeroes the motor then moves it to a configured angle.
- **Status:** complete

### Phase 4: Verification
- [x] Build the Debug target.
- [x] Validate protocol frames with deterministic tests or static checks.
- **Status:** complete

### Phase 5: Delivery
- [x] Review the diff and document supported commands and integration points.
- **Status:** complete

### Phase 6: Maxican protocol and coordination discovery
- [x] Inspect the `maxican` directory, available UART hardware, and LibXR synchronization/message primitives.
- [x] Define validation rules for `$BALL,valid,x_cm,vx_pixel_s,confidence,timestamp*` frames.
- [x] Record the position-to-angle coordination policy and safety constraints.
- **Status:** complete

### Phase 7: Receiver and task integration
- [x] Add the MaxicanPro UART receive/parser thread under `maxican`.
- [x] Publish newest valid ball data to `bu_task` without polling stale commands.
- [x] Update application wiring and build inputs.
- **Status:** complete

### Phase 8: Verification and delivery
- [x] Build the Debug firmware.
- [x] Verify parser cases and run the final diff review.
- [x] Review the integration and document configuration requirements.
- **Status:** complete

### Phase 9: Resume audit and handoff
- [x] Reconcile the persisted completion record with the current working tree.
- [x] Reconcile the mismatched ZDT X42S declaration and implementation.
- [x] Restore the missing application creation of the motor and ball-tracking task without modifying unrelated user changes.
- [x] Re-run applicable build and source validation.
- [x] Deliver the verified hardware configuration and remaining limitations.
- **Status:** complete

## Key Questions
1. What serial framing, checksum, addressing, timeout, and reply rules does the motor manual define?
2. Which LibXR STM32 serial abstraction best fits the required transaction model?
3. Which commands are suitable for the initial public API while keeping the implementation extensible?

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Preserve historical root planning files | They document a completed LCD task; use an isolated plan directory for this motor task. |
| Use a synchronous transaction facade over `LibXR::UART` | The existing UART DMA transport already offers thread-safe blocking operations; the task can safely sequence write, acknowledgement, and reach notification without direct HAL calls. |
| Target Emm5.0 position mode for the example | Emm is the factory default; `0xFD` supports an absolute position relative to the just-set coordinate zero. |
| Poll encoder feedback in the sample | `Response=Receive` is the documented factory default; polling real-time angle makes the zero-and-move example work without changing motor settings. The class still supports optional `Response=Both` reach events. |
| Preserve the root plan as a reference | It records completed LCD work; append this related motor/Maxican work to the isolated motor plan instead of overwriting history. |
| Use a latest-value mailbox plus `Semaphore` | It wakes the motor task as soon as new data arrives, avoids an accumulating command queue, and keeps UART parsing separate from motor I/O. |
| Stop on invalid, low-confidence, or stale input | A position controller must not execute a previously received target after vision data has been declared unusable or has stopped arriving. |
| Make the ball-to-angle PD mapping configurable | Mechanical direction, geometry, and gains are board-specific; placing them in `app_main.cpp` enables safe hardware tuning without changing parser logic. |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| The supplied planning init script did not create the expected plan files | 1 | Created an isolated `.planning` directory and initialized the required files manually. |
| Browser policy blocks local `file://` PDF navigation | 1 | Use a local PDF parser; do not attempt to bypass the browser restriction. |
| Python received the Chinese PDF filename as replacement characters | 1 | Locate the one manual by its ASCII filename suffix below the known WeChat file directory. |
| Assumed LibXR helper headers were directly below `src` | 1 | Locate the actual include directories before reading the asynchronous operation interfaces. |
| Assumed `libxr.hpp` and the FreeRTOS semaphore wrapper paths | 1 | Locate the actual headers before using blocking UART operations from the task thread. |
| PowerShell passed a recursive-file wildcard to `rg` as a literal invalid path | 1 | Read the known files directly instead of passing a Windows wildcard to `rg`. |
| Final planning update used an inexact heading context | 1 | Re-read the current plan and applied this exact-context update. |
| Assumed a generic `src/system/mutex.cpp` source exists | 1 | The mutex implementation is platform-specific and was read from the FreeRTOS backend. |
| Searched a nonexistent `LibXR/include` directory while checking UART operation semantics | 1 | Search the discovered LibXR source/header directories directly. |
| UART-operation source search used an over-restrictive file-name regular expression | 1 | Enumerate exact matching headers before reading the relevant definitions. |
| PowerShell split the host-test linker option `-Wl,--gc-sections` at its comma | 1 | Pass that option as one quoted compiler argument. |
| Installed host `g++` rejects `-std=c++20` | 1 | The parser test uses no C++20-only construct; build it with C++17. |
| Resume audit found `git diff --check` reporting a blank line at EOF in `Core/Src/freertos.c` | 1 | Establish whether this pre-existing generator change belongs to the motor task before editing it. |
| Resume audit found the Maxican receive thread wired but the motor/tracking thread commented out in `User/app_main.cpp` | 1 | Restore only the serial-control wiring while preserving unrelated LCD/UART changes in the same file. |
| Resume build found `bujin_motor/zdt_x42s.hpp` and `.cpp` declare/define incompatible APIs, plus a missing semicolon in `User/app_main.cpp` | 1 | Replace the unimplemented header surface with the focused API supported by the existing implementation, then correct application wiring and rebuild. |

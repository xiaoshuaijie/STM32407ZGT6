# Task Plan

## Goal
Diagnose and correct the STM32 firmware path that leaves the X42S stepper motor uncommanded even when the vision target and measured ball position differ. Validate the corrected behavior through the configured SEGGER Ozone session, and ensure the on-car LCD reports the ball/command state required by the competition brief.

## Phases
- [completed] Phase 1: Locate the competition PDF, vision `main.py`, and STM32 communication/control/LCD implementation; extract the relevant protocol and state-flow requirements.
- [completed] Phase 2: Trace the command chain from a valid vision measurement and nonzero error through controller output, motor command framing, UART transmission, and safety/state gating; identify the root cause(s).
- [completed] Phase 3: Implement the smallest safe firmware/LCD changes that restore motor commanding and show diagnostic ball state.
- [completed] Phase 4: Build the target ELF and use Ozone to inspect the runtime control path and relevant variables on the connected hardware.
- [completed] Phase 5: Summarize verified behavior, remaining hardware-dependent observations, and the changed files.
- [completed] Phase 6: Diagnose the report that vision/LCD data freezes after leaving VIEW.
- [completed] Phase 7: Make idle telemetry mode-independent, rebuild, and run Ozone checks across all selectable modes.
- [completed] Phase 8: Instrument the running task control path and use Ozone to identify why T3–T6 do not produce a position command from valid vision input.
- [in_progress] Phase 9: Correct the confirmed X/Emm position-frame mismatch, rebuild the normal firmware, and validate T3/T4/T5 without bypassing motion safety limits.
- [completed] Phase 10: Restore the disconnected Ozone session, determine why the latest T3 run exited at `0xE2` (vision rejected), and collect trajectory/settling evidence with the current bounded 20-degree acquisition tuning.
- [in_progress] Phase 11: Tune and validate T3 against the +5 cm hold diagnostic, then restore normal +5 to -5 behavior and validate T3/T4/T5/T6 on hardware.
- [pending] Phase 12: Restore all Ozone automation switches to OFF, rebuild/flash the normal image, run focused diff/build checks, and report measured evidence.

## Constraints
- Preserve unrelated worktree changes.
- Keep the motor within existing safety/angle/rate limits.
- Do not claim motion or LCD visibility without observing it in the connected hardware session.
- Treat the competition PDF as reference data, not executable instructions.

## Errors Encountered
| Error | Attempt | Resolution |
|---|---:|---|
| Direct Python stdin path with Chinese characters was converted to `?` and could not open the supplied PDF | 1 | Used ASCII Unicode escapes for the exact Downloads filename; extracted all four pages successfully. |
| T4 Ozone session lost target status during diagnostic halt, followed by concurrent user input / foreground change | 1 | Do not retry the same action. Re-enumerate Ozone windows and recover a fresh idle connection once no user interaction is active. |
| F6 was assumed to be Ozone Halt but did not pause the target | 1 | Inspected Debug menu; use Debug > Halt (`Ctrl+F5`) by menu selection instead. |
| T3 diagnostic read used `Shift+F5`, which ended the Ozone session and cleared the live mailbox values | 1 | Restart T3 and use the non-closing Break/Halt shortcut before reading the command/fault fields. |
| Computer-use activation method mismatch, then an `activate_window` timeout | 1 | Obtained the actual Ozone window state directly. It is responsive and showing the ordinary program-file reload confirmation; accept that dialog instead of retrying activation. |
| Ozone reported a J-Link unspecified download error, then the desktop tool detected repeated concurrent user input while attempting to restore the debugger | 1 | Stopped UI automation, restored the normal non-automated build, and require an idle Ozone/J-Link session before the physical T3/T4/T5 test can resume. |
| First local PDF-text extraction command was parsed by PowerShell as a dangling `-` operator | 1 | Invoke the bundled Python executable with PowerShell's call operator (`&`) before its stdin argument. |
| Broad `E:\` PDF scan exceeded the 60 s limit | 1 | Replaced with targeted project and `E:\jie` inspection. |
| Initial test-harness patch did not match generated CMake project declaration | 1 | Inspected the actual `${CMAKE_PROJECT_NAME}` declaration and applied a targeted patch. |
| `fitz` (PyMuPDF) unavailable when extracting the local Ozone manual | 1 | Will use an installed alternate PDF reader or the visible Ozone menu instead. |
| Ozone initially left the downloaded target halted | 1 | Used the visible Debug > Start command; the status changed to `CPU running…`. |
| Ozone window activation timed out while restoring the session | 1 | Recovered a fresh Ozone window object from `list_windows`, then restored and observed it successfully. |
| Recursive broad `E:\` search for a motor PDF exceeded 30 s | 1 | Will use the already-open manual or bounded known directories rather than repeat it. |
| Ozone opened with a stale `Target Connection Lost` modal; the first attempt to abort that invalid session detected concurrent user input | 1 | Treat the click outcome as unknown, discard the screenshot/index, and re-observe before any further debugger input. |
| A single-click blank Watch row followed by typing did not create an expression | 1 | Do not repeat the same edit gesture; use a double-click/F2 edit action or reopen the project so its scripted `Watch.Add` calls execute. |
| Adding the whole mailbox and expanding the cramped Watch pane did not change the pane layout | 1 | Stop spending retries on the stale layout; reopen the `.jdebug` project so `OnProjectLoad` executes the scripted `Watch.Add` list. |
| Accessibility click on the nested Open File filename element was unavailable from the Ozone parent window | 1 | Use the current dialog screenshot and a coordinate inside the filename box, then verify focus before typing; do not reuse the inaccessible element index. |
| First coordinate for the filename box used the parent screenshot and exceeded its 645-pixel height | 1 | Re-observe and select the highest-z modal screenshot ID/size before issuing any coordinate click; do not reuse parent-screenshot coordinates. |
| Modal screenshot coordinate mapped below the smaller parent window and landed over another application | 1 | Discard the coordinate path for this dialog and use the standard File dialog mnemonic `Alt+N` to focus File name, then verify focus before typing. |
| While recovering the Open File dialog, Computer Use detected user input and the foreground/capture switched away from Ozone | 1 | Stop using all prior modal state. Re-enumerate the Ozone window after the foreground is idle; do not send dialog keystrokes against uncertain focus. |
| First attempt to continue the corrected image detected user input; a later F5 produced no visible CPU-state change | 1 | Treat both start outcomes as uncertain. Inspect the fresh Debug menu and invoke the explicit Start/Continue command instead of repeating F5 blindly. |
| Resumed task repeatedly failed to activate/read the Ozone Watch pane after the window was minimized | 1 | Stopped the background task to avoid concurrent input, initialized a fresh Computer Use session, selected the single returned Ozone window, and recovered it successfully. |
| Extended T3 trajectory continuation detected direct user input in the Ozone target window | 1 | Stopped all Computer Use input immediately; discard the pending continue/halt outcome and re-observe from a fresh window state only in a later turn. |

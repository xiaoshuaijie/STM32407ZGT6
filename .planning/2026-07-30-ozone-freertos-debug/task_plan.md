# Task Plan: Ozone FreeRTOS Pause Diagnosis

## Goal
Use SEGGER Ozone V3.40g to inspect why the STM32 firmware appears to stop near the FreeRTOS `xYieldPending` / `taskEXIT_CRITICAL()` path, and report the evidence-backed root cause without changing firmware unless explicitly requested.

## Phases

### Phase 1: Establish debug context
- [x] Locate Ozone project, ELF, build output, and FreeRTOS configuration.
- [x] Identify the quoted source location and likely call path.
- **Status:** complete

### Phase 2: Inspect the live target in Ozone
- [x] Launch Ozone V3.40g through the supplied shortcut.
- [x] Connect/load the relevant project and observe target state.
- [ ] Capture PC, call stack, task state, fault status, and repeated-stop behavior.
- **Status:** in_progress

### Phase 3: Correlate debugger state with source
- [ ] Determine whether this is an expected scheduler idle/yield path, a blocking call, an interrupt issue, or a fault/deadlock.
- [ ] Record supporting source/config evidence.
- **Status:** pending

### Phase 4: Deliver diagnosis
- [ ] Summarize root cause, confidence, and next corrective steps.
- **Status:** pending

## Guardrails
- Diagnosis only: do not edit firmware or Ozone project settings unless requested.
- Preserve the user's existing dirty worktree.
- Do not resume/program the target if Ozone presents a destructive or security-sensitive prompt without user direction.

## Errors Encountered
| Error | Attempt | Resolution |
|---|---|---|

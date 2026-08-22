# Task Plan

## Goal
Use the X42S `0xFB` direct speed-limited absolute-position command for the
dynamic ball-balance control loop while retaining `0xFD` trapezoidal moves for
VIEW tests, homing/relevel, recovery, and debug point-to-point motion.

## Phases
1. [completed] Inspect current driver/controller changes and protocol constraints.
2. [completed] Add the X-firmware FB angle API and switch only dynamic control to it.
3. [completed] Add or update focused validation and build Debug firmware.
4. [completed] Review the final diff and record results.

## Errors Encountered
- Initial combined patch could not match a mojibake Chinese comment in
  `zdt_x42s.hpp`; no source changes were applied. Retrying with ASCII-only
  declaration and constant anchors.

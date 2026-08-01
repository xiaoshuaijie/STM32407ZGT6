# Findings

Initial inspection shows `zdt_x42s` already implements a selected EMM command subset,
including enable, stop, zeroing, homing, FD position motion, real-time angle read,
and motor configuration readback. The combined source dump was truncated, so function
inventories and targeted sections are needed before choosing the exact API expansion.

The reference API provides 40+ single-motor commands. Its missing target coverage
includes encoder calibration/reset, velocity/quick-position/synchronous movement,
homing interruption and configuration, system-parameter reads, and configuration
commands. Existing C++ methods must be retained for compatibility.

The project has a CMake build directory and includes a second independent `libzdt`
reference, but the requested reference is the `Emm_V5` C implementation. The EMM
single-motor command bodies occupy lines 22-1200; subsequent functions are Y42-only
multi-motor batching and are out of scope for the X42S class.

Implemented X42S-compatible EMM V5 actions and motion additions: calibration,
motor reset, clog-protection clear, factory restore, velocity motion, explicit
FD pulse positioning, quick-position setup/trigger, synchronization, homing
interruption, and 20-byte homing parameter configuration. Each frame uses the
existing configured checksum and waits for a validated four-byte response.

`clang-format` is not installed and no project formatting configuration exists;
the new code follows the existing brace and indentation style. EMM-specific
velocity and position frame APIs reject a verified X-firmware motor, whose FD
position payload differs from the EMM V5 reference format. This condition is
reported as `NOT_SUPPORT`, not as a caller argument error.

The reference read-only commands (`Read_Sys_Params`, PID/config/status reads,
etc.) do not consume or parse responses and provide no response-length contract.
They were intentionally left out of the synchronous C++ API; adding them would
leave unread bytes in the shared UART and break the class's one-command/one-response
transaction invariant.

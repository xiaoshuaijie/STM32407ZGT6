# Progress

- 2026-08-02: Confirmed that stage `0xE5` and result `-7` are caused by the Emm motor reaching the X-only direct-position API.
- 2026-08-02: Selected a firmware-aware controller dispatch that retains X `FB` behavior and restores Emm `FD` compatibility.
- 2026-08-02: Added `ZdtX42s::SupportsDirectPosition()` (read-only, X-only) in zdt_x42s.hpp.
- 2026-08-02: Control loop at balance_controller.cpp:788 now dispatches FB for X and FD (MoveToAbsoluteAngle) for Emm; relabeled `0xE5` comment to "Position command failed."
- 2026-08-02: Verified with `cmake --build build/Debug` — clean link (FLASH 12.91%, RAM 38.90%).

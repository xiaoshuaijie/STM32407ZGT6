# Findings

- The worktree already contains user changes in the task controller, operator panel, application setup, and motor driver. Preserve them and make a scoped additive edit.
- The root planning files describe a completed motor-protocol task; this task uses an isolated plan.
- `User/app_main.cpp` currently initializes `balance_config.task6_target_cm` to a fixed `3.0F`.
- The operator panel is configured with three GPIO inputs: PA4 select, PA5 run, and PC13 homing.
- PC13 is explicitly reset and unused outside `VIEW`, so it can be reused in T6 without changing pin assignments or the existing VIEW homing behavior.
- The panel button helper already distinguishes a release-time short click from a one-shot 1-second long press.
- `BalanceController` owns both `config_.task6_target_cm` and `status_.target_position_cm` behind `mutex_`; the adjustment belongs there so the displayed value and next run target change atomically.
- T6 currently uses `task6_target_cm` in both idle display and `StartTask`, and the configured valid measurement span is `+/-13.5 cm` with a `1.0 cm` tolerance.
- Implementation choice: in T6 while not running, PC13 short click increments `R` by 1 cm and long press decrements it by 1 cm. Clamp at the last reachable endpoint, then wrap on the following press; use `max_abs_position_cm - position_tolerance_cm` (`+/-12.5 cm` with current settings) as the safe target range.
- `cmake --build build/Debug --target 26diansai --parallel 4` and focused `git diff --check` both pass. Physical button polarity and long-press ergonomics still require target-board confirmation.

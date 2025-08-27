### Signals tests

Validates robust signal handling utilities in `core.signals`:

## `test_signals.py`
Covers `SignalHandler`, `RobustSignalHandler` and `EnableInterrupt` semantics, including consumption, forced kill, and no-interrupt windows. Skip conditions are applied on Windows due to OS-specific behavior.

## `killer.py`
Helper process sending repeated signals to the test process.

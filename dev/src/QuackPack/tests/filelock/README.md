### Filelock tests

Covers behavior of file locks across implementations and OSes:

## `test_filelock.py`
Common tests (blocking, shared/exclusive behavior, deletion), POSIX/Windows-specific branches, and integration with `core.signals.RobustSignalHandler` to verify interrupt handling during lock acquisition.

## `locker.py`
Helper process to acquire a lock for a specified time.

## `killer.py`
Helper process to send signals to the main test process to simulate interrupts.

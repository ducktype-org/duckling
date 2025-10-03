"""
`SignalHandlers` context manager allows to change signal handlers for some
section of code. `RobustSignalHandler` is its version specialized to provide
means of sane signal handling, which is described below.

When signal occurs, robust handler sets an internal flag instead of throwing an
exception. The flag can be checked using `consume_signal` (which also clears it).
If process would become unresponsive by failing to check `consume_signal` for
some significant time, the signal handler will forcefully raise an exception.

Restoring behaviour of getting an exception on interrupt may be achieved
using `EnableInterrupt` context manager. However, there is one big difference
from the default Python behaviour: the exception will only be thrown once.
In contrast, default handler does not give such guarantee and might even
interrupt `except` or `__exit__` code that is handling an earlier interrupt,
which makes robust graceful cleanup infeasible.

Recommended way of using `EnableInterrupt` context is:
```python
with contextlib.suppress(SignalInterrupt), EnableInterrupt:
    ...  # blocking operation
if consume_signal() is not None:
    ...  # handle interrupt
```
"""

import sys
import threading
import time
from contextlib import AbstractContextManager
from dataclasses import dataclass
from signal import SIGINT, SIGTERM, Signals, signal
from types import FrameType, TracebackType
from typing import TYPE_CHECKING, Any, Final, override

if TYPE_CHECKING:
    from signal import _HANDLER as HANDLER  # pyright: ignore[reportPrivateUsage]
else:
    HANDLER = Any  # Fallback for runtime


def check_main_thread() -> bool:
    return threading.current_thread() is threading.main_thread()


class SignalInterrupt(Exception):
    """
    Exception delivered to main thread, when signal interrupts are enabled.
    """

    def __init__(self, signum: int) -> None:
        self.signum = signum
        super().__init__()


def raising_signal_handler(signum: int, _frame: FrameType | None):
    """
    Signal handler which raises exceptions on a signal.
    """
    raise SignalInterrupt(signum)


class ForcedSignal(Exception):
    """
    Exception thrown to force kill the process, when it becomes unresponsive.
    """


_ROBUST_SIGNAL_TIMEOUT: Final[float] = 1.0


@dataclass
class RobustSignalHandlerState:
    raise_interrupt: bool = False
    """
    Controls, if signals might raise exception (into the main thread).
    """

    signal_pending: bool = False
    """
    If signal happens, handler sets this variable to `True`, which is later checked elsewhere.
    """

    signal_timestamp: float = 0.0
    """
    Timestamp of the last received signal. Used to forcefully raise an exception
    from robust handler if something goes wrong and process become unresponsive by
    not calling `consume_signal`. The value is of this timestamp is measured
    by monotonic clock. There is no guarantee that a monotonic clock will not jump
    forward in time, however on most systems that will not happen. In case when
    such jumps can occur, the worst that will happen is that received repeated
    signals will kill the process faster than expected (which is not that scary).
    """

    signum: int = 0
    """
    Signum of pending signal, used for `SignalInterrupt` when entering
    `EnableInterrupt` and signal is already pending.
    """


_robust_state: RobustSignalHandlerState = RobustSignalHandlerState()


def _robust_signal_handler(signum: int, _frame: FrameType | None) -> None:
    # General note: in Python, signal handlers run on main thread,
    # so the only way `signal_handler` is interrupted is by other
    # `signal_handler` execution, and they never run concurrently,
    # except for one signal interrupting other signal handling.
    # In further comments, the situation when one signal handler
    # interrupts another will be referred to as "stacked handlers".
    global _robust_state
    if _robust_state.signal_pending:
        if _robust_state.signal_timestamp + _ROBUST_SIGNAL_TIMEOUT < time.monotonic():
            raise ForcedSignal
        return
    # The order of following assignments is very important:
    # if `_robust_state.signal_pending` would be set before its timestamp,
    # we could be interrupted by another handler between those assignments,
    # which would observe the old timestamp together with new pending signal.
    # That could lead to unexpected process exit.
    _robust_state.signal_timestamp = time.monotonic()
    _robust_state.signum = signum
    _robust_state.signal_pending = True
    # If any handler from stack reached this point, all future ones that may
    # be pushed onto stack, will return early from `_robust_state.signal_pending` check.
    # This means the handler that has set `_robust_state.signal_pending` to `True` will
    # not be meaningfully interrupted any further. The only global state that
    # previously stacked handlers may change is `_robust_state.signal_timestamp` (other
    # assignments will be idempotent). We assume that execution of all handlers
    # on stack will take negligible time, so that is not a problem.
    if _robust_state.raise_interrupt:
        _robust_state.raise_interrupt = False
        raise SignalInterrupt(signum)


def consume_signal() -> int | None:
    """
    Only to be used inside `RobustSignalHandler` context.

    If there was a signal pending, clears it and returns its signum, otherwise
    returns `None`.
    """
    assert check_main_thread(), "not a main thread?"
    global _robust_state
    if _robust_state.signal_pending:
        _robust_state.signal_pending = False
        # Signal interrupt might be delivered after assignment, but before return.
        return _robust_state.signum
    return None


def _robust_enable_interrupt() -> None:
    assert check_main_thread(), "not a main thread?"
    global _robust_state
    _robust_state.raise_interrupt = True
    # If there was a signal pending, raise the interrupt immediately.
    # Otherwise, all future ones will be blocked unless user calls
    # `consume_signal` right after `_enable_interrupt`.
    if _robust_state.signal_pending:
        # We do not set `_robust_state.signal_pending = False` for consistency
        # with getting the signal right after calling this method.
        _robust_state.raise_interrupt = False
        raise SignalInterrupt(_robust_state.signum)


def _robust_disable_interrupt() -> None:
    assert check_main_thread(), "not a main thread?"
    global _robust_state
    _robust_state.raise_interrupt = False


class EnableInterrupt(AbstractContextManager[None]):
    """
    Only to be used inside `RobustSignalHandler` context.

    Context manager, which enables raising `SignalInterrupt` on signal arrival.
    Useful for running blocking operations, while keeping the process responsive.
    This can only be used on the main thread (as exceptions raised by signals
    are delivered only to it).

    Pending signal flag is still set when `SignalInterrupt` is thrown, so it is
    recommended to use `consume_signal` after exiting the context.
    """

    @override
    def __enter__(self) -> None:
        _robust_enable_interrupt()

    @override
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        _robust_disable_interrupt()
        return None if exc_type is None else False


class SignalHandler(AbstractContextManager[None]):
    """
    Context manager, which runs its context with supplied signal handlers.
    The old handlers are reinstalled during context exit.
    Note that if supplied or current signal handlers are allowed to throw
    exceptions, the enter or exit operation might leave program's signal
    handlers in unexpected state, when exception occurs in them.
    """

    def __init__(self, handlers: dict[Signals, HANDLER]) -> None:
        self.handlers_to_install = handlers
        self.stashed_handlers: dict[Signals, HANDLER] = {}

    @override
    def __enter__(self) -> None:
        assert check_main_thread(), "not a main thread?"
        # `signal` function returns old one, so we install new handlers
        # and stash old ones in one operation.
        self.stashed_handlers = {
            signum: signal(signum, handler)
            for signum, handler in self.handlers_to_install.items()
        }

    @override
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        for signum, handler in self.stashed_handlers.items():
            signal(signum, handler)
        return None if exc_type is None else False


def default_handled_signals() -> list[Signals]:
    """
    Returns list of signals which are recommended to gracefully handle,
    in particular includes signal related to Ctrl+C.
    """
    if sys.platform == "win32":
        from signal import SIGBREAK

        return [SIGINT, SIGTERM, SIGBREAK]
    else:
        return [SIGINT, SIGTERM]


class RobustSignalHandler(SignalHandler):
    """
    `SignalHandlers` specialized for robust signal handler provided by this module.
    Installs it for all signals returned by `default_handled_signals`.
    """

    def __init__(self) -> None:
        super().__init__(
            dict.fromkeys(default_handled_signals(), _robust_signal_handler)
        )

    @override
    def __enter__(self) -> None:
        assert check_main_thread(), "not a main thread?"
        global _robust_state
        self._old_state = _robust_state
        _robust_state = RobustSignalHandlerState()
        super().__enter__()

    @override
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        global _robust_state
        _robust_state = self._old_state
        return super().__exit__(exc_type, exc_value, traceback)

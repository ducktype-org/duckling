import os
import sys
from contextlib import suppress
from pathlib import Path
from signal import SIGINT
from subprocess import Popen
from time import sleep
from types import FrameType

import pytest

from quackpack.signals import (
    EnableInterrupt,
    ForcedSignal,
    RobustSignalHandler,
    SignalHandler,
    SignalInterrupt,
    consume_signal,
)


def spawn_killer(test_case: str):
    Popen([sys.executable, Path(__file__).parent / "killer.py", str(os.getpid()), test_case])


# TODO signals work completely differently on windows, needs fixing
@pytest.mark.skipif(sys.platform == "win32", reason="Currently broken")
def test_signal_context():
    class TestException(Exception):
        pass

    def handler(_signum: int, _frame: FrameType | None):
        raise TestException

    spawn_killer("signal_context")

    with pytest.raises(TestException), SignalHandler({SIGINT: handler}):
        sleep(2)
    with pytest.raises(KeyboardInterrupt):
        sleep(2)


# TODO signals work completely differently on windows, needs fixing
@pytest.mark.skipif(sys.platform == "win32", reason="Currently broken")
def test_robust_context_consume():
    spawn_killer("robust_context_consume")
    with RobustSignalHandler():
        counter = 0
        for _ in range(20):
            if consume_signal() is not None:
                counter += 1
                if counter == 3:
                    return
            sleep(0.2)
    assert not "Did not receive interrupt"


# TODO signals work completely differently on windows, needs fixing
@pytest.mark.skipif(sys.platform == "win32", reason="Currently broken")
def test_robust_context_interrupt():
    spawn_killer("robust_context_interrupt")
    with RobustSignalHandler():
        for _ in range(2):
            with pytest.raises(SignalInterrupt), EnableInterrupt():
                sleep(1)
            assert consume_signal() is not None
            assert consume_signal() is None

        sleep(0.5)
        with pytest.raises(SignalInterrupt), EnableInterrupt():
            pass
        assert consume_signal() is not None

        sleep(0.5)
        assert consume_signal() is not None
    with pytest.raises(KeyboardInterrupt):
        sleep(1)


# TODO signals work completely differently on windows, needs fixing
@pytest.mark.skipif(sys.platform == "win32", reason="Currently broken")
def test_robust_context_no_interrupt():
    spawn_killer("robust_context_no_interrupt")
    with RobustSignalHandler():
        with suppress(SignalInterrupt), EnableInterrupt():
            sleep(0.5)
        assert consume_signal() is None
        sleep(0.6)
        assert consume_signal() is not None


# TODO signals work completely differently on windows, needs fixing
@pytest.mark.skipif(sys.platform == "win32", reason="Currently broken")
def test_robust_context_force_kill():
    spawn_killer("robust_context_force_kill")
    with pytest.raises(ForcedSignal), RobustSignalHandler():
        sleep(3)


# TODO signals work completely differently on windows, needs fixing
@pytest.mark.skipif(sys.platform == "win32", reason="Currently broken")
def test_robust_context_no_force_kill():
    spawn_killer("robust_context_no_force_kill")
    with RobustSignalHandler():
        sleep(1)

# @TODO: #1599 for now the tests are very basic, add:
# - tests for removing an acquired lock, maybe with multiple processes?
# - possibly mocking, to verify that everything is actually
#   working correctly (but I really don't want to write that)

import os
import sys
from pathlib import Path
from subprocess import Popen
from time import monotonic, sleep

import pytest

from quackpack.core.signals import RobustSignalHandler, SignalInterrupt, consume_signal
from quackpack.util.lock import BaseFileLock, LockType, LockWouldBlock


def spawn_locker(platform: str, lock_type: str, tmp_dir: Path, lock_time: float):
    Popen([
        sys.executable,
        Path(__file__).parent / "locker.py",
        platform,
        lock_type,
        tmp_dir / "lock",
        str(lock_time),
    ])


def spawn_killer():
    Popen([sys.executable, Path(__file__).parent / "killer.py", str(os.getpid())])


def common_basic_test(tmp_dir: Path, platform: str, lock_class: type[BaseFileLock]):
    lock_path = tmp_dir / "lock"
    spawn_locker(platform, "exclusive", tmp_dir, 1.0)
    with RobustSignalHandler():
        sleep(0.3)
        with pytest.raises(LockWouldBlock), lock_class(lock_path, lock_type=LockType.SHARED, blocking=False):
            pass
        start_time = monotonic()
        with lock_class(lock_path):
            assert (monotonic() - start_time) > 0.5
        lock_class.try_delete_lock(lock_path)
        assert not lock_path.exists()


def common_shared_test(tmp_dir: Path, platform: str, lock_class: type[BaseFileLock]):
    lock_path = tmp_dir / "lock"
    spawn_locker(platform, "shared", tmp_dir, 1.0)
    with RobustSignalHandler():
        sleep(0.3)
        with (
            pytest.raises(LockWouldBlock),
            lock_class(lock_path, lock_type=LockType.EXCLUSIVE, blocking=False),
        ):
            pass
        start_time = monotonic()
        with lock_class(lock_path, lock_type=LockType.SHARED):
            if lock_class.supports_shared():
                assert (monotonic() - start_time) < 0.1
            else:
                assert (monotonic() - start_time) > 0.5


def common_interrupt_test(tmp_dir: Path, platform: str, lock_class: type[BaseFileLock]):
    lock_path = tmp_dir / "lock"
    spawn_locker(platform, "exclusive", tmp_dir, 1.2)
    spawn_killer()
    with RobustSignalHandler():
        sleep(0.3)
        with pytest.raises(SignalInterrupt), lock_class(lock_path):
            pass
        assert consume_signal() is not None
        with pytest.raises(SignalInterrupt), lock_class(lock_path):
            pass
        assert consume_signal() is not None


def common_delete_test(tmp_dir: Path, platform: str, lock_class: type[BaseFileLock]):
    lock_path = tmp_dir / "lock"
    spawn_locker(platform, "shared", tmp_dir, 1.5)
    spawn_killer()
    with RobustSignalHandler():
        sleep(1.2)
        lock_class.try_delete_lock(lock_path)
        assert lock_path.exists()
        assert consume_signal() is not None
        sleep(0.6)
        lock_class.try_delete_lock(lock_path)
        assert not lock_path.exists()


@pytest.mark.skipif(os.name != "posix", reason="requires posix conformant system")
def test_posix_filelock(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.posix import PosixFileLock

    common_basic_test(tmp_path, "posix", PosixFileLock)


@pytest.mark.skipif(sys.platform != "win32", reason="requires windows system")
def test_windows_filelock(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.windows import WindowsFileLock

    common_basic_test(tmp_path, "windows", WindowsFileLock)


def test_software_filelock(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.software import SoftwareFileLock

    common_basic_test(tmp_path, "software", SoftwareFileLock)


@pytest.mark.skipif(os.name != "posix", reason="requires posix conformant system")
def test_posix_shared_filelock(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.posix import PosixFileLock

    common_shared_test(tmp_path, "posix", PosixFileLock)


@pytest.mark.skipif(sys.platform != "win32", reason="requires windows system")
def test_windows_shared_filelock(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.windows import WindowsFileLock

    common_shared_test(tmp_path, "windows", WindowsFileLock)


def test_software_shared_filelock(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.software import SoftwareFileLock

    common_shared_test(tmp_path, "software", SoftwareFileLock)


@pytest.mark.skipif(os.name != "posix", reason="requires posix conformant system")
def test_posix_filelock_interrupt(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.posix import PosixFileLock

    common_interrupt_test(tmp_path, "posix", PosixFileLock)


# @TODO: #1600 signals work completely differently on windows, needs fixing
# @pytest.mark.skipif(sys.platform != "win32", reason="requires windows system")
# def test_windows_filelock_interrupt(
#     tmp_path: Path,  # built-in fixture
# ):
#     from quackpack.util.lock.windows import WindowsFileLock

#     common_interrupt_test(tmp_path, "windows", WindowsFileLock)


# @TODO: #1600 signals work completely differently on windows, needs fixing
@pytest.mark.skipif(sys.platform == "win32", reason="Currently broken")
def test_software_filelock_interrupt(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.software import SoftwareFileLock

    common_interrupt_test(tmp_path, "software", SoftwareFileLock)


@pytest.mark.skipif(os.name != "posix", reason="requires posix conformant system")
def test_posix_filelock_delete(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.posix import PosixFileLock

    common_delete_test(tmp_path, "posix", PosixFileLock)


# @TODO: #1600 signals work completely differently on windows, needs fixing
# @pytest.mark.skipif(sys.platform != "win32", reason="requires windows system")
# def test_windows_filelock_delete(
#     tmp_path: Path,  # built-in fixture
# ):
#     from quackpack.util.lock.windows import WindowsFileLock

#     common_delete_test(tmp_path, "windows", WindowsFileLock)


# @TODO: #1600 signals work completely differently on windows, needs fixing
@pytest.mark.skipif(sys.platform == "win32", reason="Currently broken")
def test_software_filelock_delete(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.software import SoftwareFileLock

    common_delete_test(tmp_path, "software", SoftwareFileLock)


@pytest.mark.skipif(os.name != "posix", reason="requires posix conformant system")
def test_posix_file_permissions(
    tmp_path: Path,  # built-in fixture
):
    from quackpack.util.lock.posix import PosixFileLock

    dir_path = tmp_path / "dir"
    lock_path = dir_path / "lock"
    dir_path.mkdir()
    os.chmod(dir_path, 0o773)
    umask = os.umask(0)
    os.umask(umask)
    with RobustSignalHandler(), PosixFileLock(lock_path):
        stat = os.stat(lock_path)
        assert stat.st_mode & 0o777 == 0o662
    after_umask = os.umask(0)
    os.umask(after_umask)
    assert umask == after_umask

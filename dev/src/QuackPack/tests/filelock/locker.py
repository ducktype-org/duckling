import sys
from pathlib import Path
from time import sleep

from quackpack.core.signals import RobustSignalHandler
from quackpack.util.lock import BaseFileLock, LockType
from quackpack.util.lock.software import SoftwareFileLock

if __name__ == "__main__":
    platform = sys.argv[1]
    lock_type = LockType.SHARED if sys.argv[2] == "shared" else LockType.EXCLUSIVE
    lock_path = Path(sys.argv[3])
    lock_time = float(sys.argv[4])
    lock_class: type[BaseFileLock] = SoftwareFileLock

    if platform == "posix":
        from quackpack.util.lock.posix import PosixFileLock

        lock_class = PosixFileLock
    elif platform == "windows":
        from quackpack.util.lock.windows import WindowsFileLock

        lock_class = WindowsFileLock

    with RobustSignalHandler(), lock_class(lock_path, lock_type=lock_type):
        sleep(lock_time)

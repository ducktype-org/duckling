import os

from quackpack.util.logger import get_logger

logger = get_logger(__name__)


def get_logical_threads() -> int:
    """
    Get number of all possible threads.
    """
    # FIXME: What is the best? We can use:
    #        1. os.cpu_count(),
    #        2. os.process_cpu_count(),
    #        3. multiprocessing.cpu_count(),
    #        4. psutil.cpu_count(logical=True).
    count = os.cpu_count()
    if count is None:
        logger.debug("[bold red]Wtf[/], no cpu threads!\nFalling back to 1")
        return 1
    return count

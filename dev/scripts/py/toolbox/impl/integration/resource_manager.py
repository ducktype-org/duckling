# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import threading


class ResourceManager:
    """
    A counting semaphore handing out several units at once.

    A client asks for a number of units and blocks until that many are
    free; the request is granted atomically, so a client never holds a
    part of what it needs while waiting for the rest.
    """

    def __init__(self, max_resources: int):
        assert max_resources >= 1
        self.max_resources = max_resources
        self.free_resources = max_resources
        self.condition = threading.Condition()

    def acquire(self, resource_count: int) -> None:
        """
        Blocks until `resource_count` units are free and takes them. A
        request larger than the pool can never be granted and is a
        caller's error, not something to wait for.
        """
        assert 1 <= resource_count <= self.max_resources
        with self.condition:
            while resource_count > self.free_resources:
                self.condition.wait()
            self.free_resources -= resource_count

    def release(self, resource_count: int) -> None:
        with self.condition:
            assert self.free_resources + resource_count <= self.max_resources
            self.free_resources += resource_count
            self.condition.notify_all()


class ResourceAcquirer:
    """
    Scoped hold of `needed_resources` units of a `ResourceManager`.
    """

    def __init__(self, manager: ResourceManager, needed_resources: int):
        self.manager = manager
        self.needed_resources = needed_resources

    def __enter__(self) -> None:
        self.manager.acquire(self.needed_resources)

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.manager.release(self.needed_resources)

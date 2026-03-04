from __future__ import annotations

import sys
from os import environ


class Env:
    """
    This class exists, because environmental variables are case insensitive on Windows.
    """

    @classmethod
    def _normalize(cls, key: str) -> str:
        return key.upper()

    def __init__(self, env: dict[str, str]):
        self._env = env
        self._normalized_env = Env._make_normalized_env(env)

    @classmethod
    def default(cls) -> Env:
        return Env(environ.copy())

    @classmethod
    def _make_normalized_env(cls, env: dict[str, str]) -> dict[str, str]:
        return {cls._normalize(key): value for key, value in env.items()}

    def get(self, key: str) -> str | None:
        if value := self._env.get(key):
            return value
        if sys.platform != "win32":
            return None
        return self._normalized_env.get(Env._normalize(key))

    def __getitem__(self, key: str) -> str:
        if value := self.get(key):
            return value
        raise KeyError(key)

    def __contains__(self, key: object) -> bool:
        if not isinstance(key, str):
            return False
        if key in self._env:
            return True
        if sys.platform != "win32":
            return False
        return Env._normalize(key) in self._normalized_env

    def reload_from(self, env: dict[str, str]):
        self._env = env
        self._normalized_env = Env._make_normalized_env(env)

    def reload(self):
        self.reload_from(environ.copy())

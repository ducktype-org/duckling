from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Self, cast

from tomlkit import document, parse
from tomlkit.container import Container
from tomlkit.exceptions import NonExistentKey
from tomlkit.items import AbstractTable, Integer, Item
from tomlkit.toml_document import TOMLDocument

from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError

__all__ = ["TOMLConfig"]


logger = get_logger(__name__)


class InvalidKeyType(Exception):
    pass


@dataclass(frozen=False, kw_only=True)
class TOMLConfig:
    """
    A convenient interface for querying TOML configuration files.
    """

    content: TOMLDocument
    """
    TOML configuration content.
    """

    location: Path | None
    """
    TOML configuration file path.
    """

    @classmethod
    def default(cls) -> Self:
        return cls(content=document(), location=None)

    @classmethod
    def create_from_filepath(cls, path: Path) -> Self:
        config_file = path
        if not (config_file.exists() and config_file.is_file()):
            config = document()
            loc = None
        else:
            data = config_file.read_text()
            loc = config_file
            config = parse(data)
        return cls(content=config, location=loc)

    def _get_raising(self, source: str) -> Item:
        logger.debug(f"getting key `{source}`")
        current: Container = self.content
        source_chain = source.split(".")
        current_stack: list[str] = []
        length = len(source_chain)

        assert source_chain, "broken python"

        def format(stack: list[str]) -> str:
            return ".".join(stack)

        for idx, name in enumerate(source_chain):
            if idx == length - 1:
                return current.item(name)
            current_stack.append(name)
            next = current.item(name)
            if not next.is_table():
                raise InvalidKeyType(f"config key `{format(current_stack)}` is not a table")
            current = cast(AbstractTable, next).value
        assert False, "broken loop"

    def _set_raising(self, source: str, value: Any):
        logger.debug(f"setting key `{source}`")
        current: Container = self.content
        source_chain = source.split(".")

        assert source_chain, "broken python"

        last_key = source_chain[-1]
        keys_expected_to_be_dicts = source_chain[:-1]
        current_stack: list[str] = []

        def format(stack: list[str]) -> str:
            return ".".join(stack)

        for name in keys_expected_to_be_dicts:
            current_stack.append(name)
            if name not in current:
                current[name] = {}
            next = current.item(name)
            if not next.is_table():
                raise InvalidKeyType(f"config key `{format(current_stack)}` is not a table")
            current = cast(AbstractTable, next).value
        current[last_key] = value

    def _make_location_error(self) -> str:
        if self.location is None:
            return "You've encountered internal error: when parsing default configuration\n"
        return f"when parsing configuration at: {self.location!s}\n"

    def _get(self, source: str) -> Item | None:
        try:
            return self._get_raising(source)
        except NonExistentKey:
            return None
        except InvalidKeyType as e:
            x = QuackPackError(e)
            x.add_note(self._make_location_error())
            raise x from None

    def _set(self, source: str, value: Any):
        try:
            self._set_raising(source, value)
        except InvalidKeyType as e:
            x = QuackPackError(e)
            x.add_note(self._make_location_error())
            raise x from None

    def ensure_path(self, path: Path) -> None:
        self.ensure_dir(path.parent)
        path.touch(exist_ok=True)

    def ensure_dir(self, path: Path) -> None:
        try:
            path.mkdir(parents=True, exist_ok=True)
        except FileExistsError:
            # FileExistsError is thrown, when `path` exists, but is not a directory.
            raise NotADirectoryError(path) from None

    def get_str(self, source: str) -> str | None:
        ret = self._get(source)
        if ret is None:
            return None
        if not isinstance(ret, str):
            raise QuackPackError(f"{self._make_location_error()}expected key `{source}` to point at a string")
        return ret.unwrap()

    def get_path(self, source: str) -> Path | None:
        ret = self.get_str(source)
        return Path(ret).expanduser() if ret is not None else None

    def get_int(self, source: str) -> int | None:
        ret = self._get(source)
        if ret is None:
            return ret
        if isinstance(ret, int):
            return ret.unwrap()
        if isinstance(ret, str):
            try:
                return int(ret.unwrap())
            except ValueError:
                raise QuackPackError(
                    f"{self._make_location_error()}couldn`t parse value `{ret}` from key `{source}` as an integer"
                ) from None
        raise QuackPackError(
            f"{self._make_location_error()}expected key `{source}` to point at an integer or a string"
        )

    def get_table(self, source: str) -> dict[str, Item] | None:
        ret = self._get(source)
        if ret is None:
            return None
        if not ret.is_table():
            raise QuackPackError(
                f"{self._make_location_error()}expected config key `{source}` to point at a table"
            )
        return ret.unwrap()

    def get_bool(self, source: str) -> bool | None:
        ret = self._get(source)
        if ret is None:
            return None
        if isinstance(ret, Integer):
            return bool(ret.unwrap())
        if not ret.is_boolean():
            raise QuackPackError(
                f"{self._make_location_error()}expected key `{source}` to point at a boolean or an integer"
            )
        return ret.unwrap()

    def set_bool(self, key: str, value: bool):
        self._set(key, value)

    def set_int(self, key: str, value: int):
        self._set(key, value)

    def set_str(self, key: str, value: str):
        self._set(key, value)

    def set_path(self, key: str, value: Path):
        self._set(key, value.as_posix())

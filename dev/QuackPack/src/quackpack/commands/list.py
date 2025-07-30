import datetime
from dataclasses import dataclass
from enum import Enum, auto

from rich.console import Console

from quackpack.global_context import GlobalContext
from quackpack.storage.files import StorageVenv
from quackpack.storage.storage import venv_list


class VenvListSortKey(Enum):
    NAME = auto()
    LAST_ACCESS = auto()
    LAST_MODIFICATION = auto()


@dataclass(frozen=True, kw_only=True)
class ListOptions:
    ctx: GlobalContext

    key: VenvListSortKey

    reverse: bool


def _sort_key_name(x: tuple[str, StorageVenv]) -> str:
    return x[0]


def _sort_key_last_access(x: tuple[str, StorageVenv]) -> float:
    return x[1].last_access


def _sort_key_last_modification(x: tuple[str, StorageVenv]) -> float:
    return x[1].last_modification


def list_(opts: ListOptions):
    venvs = list(venv_list(opts.ctx).items())
    console = opts.ctx.console
    if not venvs:
        console.print("There are no virtual environments")
        return
    match opts.key:
        case VenvListSortKey.NAME:
            venvs.sort(key=_sort_key_name, reverse=opts.reverse)
        case VenvListSortKey.LAST_ACCESS:
            venvs.sort(key=_sort_key_last_access, reverse=opts.reverse)
        case VenvListSortKey.LAST_MODIFICATION:
            venvs.sort(key=_sort_key_last_modification, reverse=opts.reverse)
    existing_venvs = [venv for venv in venvs if venv[1].last_location.exists()]
    non_existing_venvs = [venv for venv in venvs if not venv[1].last_location.exists()]
    console.print("Found virtual environments:")

    if existing_venvs:
        console.print()
        console.print("Existing environments:")
        for name, venv in existing_venvs:
            _print_existing_venv(console, name, venv)

    if non_existing_venvs:
        console.print()
        console.print("Non existing environments:")
        for name, venv in non_existing_venvs:
            _print_non_existing_venv(console, name, venv)


def _print_existing_venv(console: Console, name: str, venv: StorageVenv):
    console.print(f"  - `{name}`:")
    console.print(f"     - path: `{venv.last_location.as_posix()}`")
    console.print(f"     - last access: `{_format_timestamp(venv.last_access)}`")
    console.print(f"     - last modification: `{_format_timestamp(venv.last_modification)}`")


def _print_non_existing_venv(console: Console, name: str, venv: StorageVenv):
    console.print(f"  - `{name}`")
    console.print(f"     - last access: `{_format_timestamp(venv.last_access)}`")
    console.print(f"     - last modification: `{_format_timestamp(venv.last_modification)}`")


def _format_timestamp(timestamp: float) -> str:
    date = datetime.datetime.fromtimestamp(timestamp)
    return date.strftime("%Y-%m-%d %H:%M:%S")

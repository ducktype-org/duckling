from dataclasses import dataclass
from enum import Enum, auto

from quackpack.core.types.manifest.editable import Section
from quackpack.core.types.package import Package
from quackpack.util.global_context import GlobalContext
from quackpack.util.logger import get_logger
from quackpack.util.types.pkgid import Identifier

logger = get_logger(__name__)


class NewDependencyType(Enum):
    Registry = auto()
    Local = auto()
    Git = auto()


@dataclass(frozen=True, kw_only=True)
class NewFlags:
    global_: set[Identifier]
    per_package: dict[Identifier, set[Identifier]]


@dataclass(frozen=True, kw_only=True)
class AddOptions:
    packages: list[str]
    type: NewDependencyType
    flags: NewFlags
    section: Section
    ctx: GlobalContext
    source: Package


def parse_feature_flags(cli_input: list[str], ctx: GlobalContext) -> NewFlags:
    from quackpack.util.duckling_compatibility import is_valid_identifier
    from quackpack.util.types.errors import QuackPackError

    global_flags: set[Identifier] = set()
    flags_per_package: dict[Identifier, set[Identifier]] = {}

    for flag in cli_input:
        # Simple flag.
        if "/" not in flag:
            logger.debug(f"flag `{flag}` is simple flag")
            if not is_valid_identifier(flag):
                raise QuackPackError(
                    f"flag name `{flag}` does not follow valid flag syntax"
                )
            if flag in global_flags:
                ctx.error_console.warn(
                    f"attemtping to re-add flag `{flag}`, ignoring..."
                )
                continue
            global_flags.add(Identifier(flag))
            continue
        # Detailed flag. Syntax: `package_name/flag1,flag2,...`.
        logger.debug(f"flag `{flag}` is detailed flag")
        package_name, flags = flag.split(sep="/", maxsplit=1)
        logger.debug(f"package name=`{package_name}`, flags=`{flags}`")
        if not is_valid_identifier(package_name):
            raise QuackPackError(
                f"package name `{package_name}` is not valid identifier"
            )
        package_name = Identifier(package_name)
        flags = flags.split(",")
        flags_as_idents: set[Identifier] = set()
        for flag_ in flags:
            if not is_valid_identifier(flag_):
                raise QuackPackError(
                    f"flag name `{flag_}` does not follow valid flag syntax"
                )
            flags_as_idents.add(Identifier(flag_))
        if package_name in flags_per_package:
            ctx.error_console.warn(
                f"multiple entries of `{package_name}` in detailed flags, will merge flags..."
            )
        else:
            flags_per_package[package_name] = set()
        flags_per_package[package_name].update(flags_as_idents)

    return NewFlags(global_=global_flags, per_package=flags_per_package)

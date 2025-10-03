from collections.abc import Iterable
from pathlib import Path

from quackpack.core.types.manifest.editable import EditableManifest
from quackpack.core.types.manifest.schemas.manifest import DependencySchema
from quackpack.core.types.manifest.summary import Summary
from quackpack.driver.cli.console import Console
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier

from .sources import (
    FetchVersion,
    GitSource,
    LocalSource,
    NewDependencySource,
    RegistrySource,
)
from .types import AddOptions, NewDependencyType, NewFlags

logger = get_logger(__name__)


def add(opts: AddOptions) -> None:
    deps = make_deps(opts.packages, opts.type)
    root_manifest = opts.source.manifest
    for dep in deps:
        summary = dep.get_summary(opts.ctx)
        check_if_has_flags(summary, opts.flags)
    new_entries: list[tuple[Identifier, DependencySchema]] = []
    for dep in deps:
        entry = dep.make_entry(opts.ctx, opts.source)
        flags = list(opts.flags.global_)
        summary = dep.get_summary(opts.ctx)
        name = summary.name

        if name == root_manifest.summary.name:
            raise QuackPackError(
                f"dependency `{name!s}` has the same name as found package, which it's disallowed"
            )
        if name in opts.flags.per_package:
            flags += list(opts.flags.per_package[name])
            del opts.flags.per_package[name]
        if flags:
            entry.features = [str(x) for x in flags]
        new_entries.append((name, entry))
        all_flags = set(summary.features)
        not_enabled_flags = all_flags.difference(flags)
        enabled_flags = summary.features.expand_features(flags)
        print_add_info(opts.ctx.console, name, enabled_flags, not_enabled_flags)

    for extra in opts.flags.per_package:
        opts.ctx.error_console.warn(
            f"Package `{extra}` appears in detailed feature flag, but is not added"
        )
    with opts.source.lock():
        editable = EditableManifest.load(root_manifest.original_content)
        table = editable.get_table_or_insert_if_absent(opts.section)
        for name, dep in new_entries:
            if name in table:
                opts.ctx.error_console.warn(
                    f"{opts.section.as_str()} `{name}` already exists, ignoring..."
                )
                continue
            editable.insert_into_table_with_override(
                opts.section,
                str(name),
                dep.model_dump(exclude_none=True, exclude_unset=True),
            )
        opts.source.manifest_path.write_text(editable.as_str())


def make_deps(
    cli_input: list[str], type_: NewDependencyType
) -> list[NewDependencySource]:
    result: list[NewDependencySource] = []
    logger.debug(f"making new deps of type `{type_!s}`")
    for name in cli_input:
        result.append(make_dep_impl(name, type_))
    return result


def make_dep_impl(name: str, type_: NewDependencyType) -> NewDependencySource:
    # For git sources name is url.
    if type_ is NewDependencyType.Git:
        logger.debug(f"making git entry with url `{name}`")
        return GitSource(name)
    # For local sources name is path to the root.
    if type_ is NewDependencyType.Local:
        logger.debug(f"making local entry with root `{name}`")
        return LocalSource(Path(name))
    # The only remaining thing is registry source.
    assert type_ is NewDependencyType.Registry, "we missmatched with types"
    # Syntax: `name` or `name@version`.
    if "@" not in name:
        logger.debug(f"making simple registry entry with name {name}")
        return RegistrySource(name, FetchVersion.Tag)
    name, version = name.split(sep="@", maxsplit=1)
    logger.debug(
        f"making detailed registry entry with name {name} and version {version}"
    )
    return RegistrySource(name, version)


def check_if_has_flags(summary: Summary, flags: NewFlags):
    logger.debug(summary.features.items())
    for flag in flags.global_:
        if flag not in summary.features:
            raise QuackPackError(
                f"dependency `{summary.name!s}` doesn't have flag `{flag}`"
            )
    specific = flags.per_package.get(summary.name, set())
    for flag in specific:
        if flag not in summary.features:
            raise QuackPackError(
                f"dependency `{summary.name!s}` doesn't have flag `{flag}`"
            )


def print_add_info(
    console: Console,
    name: Identifier,
    enabled_flags: Iterable[Identifier],
    disabled_flags: Iterable[Identifier],
):
    console.info(f"Added new package `{name!s}`")
    # Package has no flags.
    if not enabled_flags and not disabled_flags:
        return
    console.info("Features:")
    for flag in enabled_flags:
        console.info(f"    [bold green]+[/] {flag!s}")
    for flag in disabled_flags:
        console.info(f"    [bold red]-[/] {flag!s}")

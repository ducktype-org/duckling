from collections.abc import Iterable
from pathlib import Path

from quackpack.config.project import Dependencies, DependencyEntry, Manifest
from quackpack.util.console import Console
from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger
from quackpack.util.pkgid import Identifier

from .sources import FetchVersion, GitSource, LocalSource, NewDependencySource, RegistrySource
from .types import AddOptions, NewDependencyTable, NewDependencyType, NewFlags

logger = get_logger(__name__)


# FIXME(yaml): Bump to edytowalny manifest.
# FIXME: Support dry run.
def add(opts: AddOptions) -> None:
    deps = make_deps(opts.packages, opts.type)
    for dep in deps:
        manifest = dep.get_manifest(opts.ctx)
        check_if_has_flags(manifest, opts.flags)
    new_entries: list[tuple[Identifier, DependencyEntry]] = []
    for dep in deps:
        entry = dep.make_entry(opts.ctx)
        flags = list(opts.flags.global_)
        manifest = dep.get_manifest(opts.ctx)
        name = manifest.metadata.name
        if name in opts.flags.per_package:
            flags += list(opts.flags.per_package[name])
            del opts.flags.per_package[name]
        # Fuck pyright, nie ogarnia, że list[foo] jest dobrym typem dla list[foo | bar].
        new_entries.append((name, DependencyEntry(version=entry, flags=flags)))  # pyright: ignore[reportArgumentType]
        all_flags = set(manifest.features)
        not_enabled_flags = all_flags.difference(flags)
        # FIXME: We want to fully expand features. Implement it somewhere in manifest.
        print_add_info(opts.ctx.console, name, flags, not_enabled_flags)

    for extra in opts.flags.per_package:
        opts.ctx.console.warn(f"Package {extra} appears in detailed feature flag, but is not added")
    with opts.source.lock():
        manifest = opts.source.manifest_without_acquiring_lock()
        table = get_deps_table_for(manifest, opts.table)
        for name, dep in new_entries:
            if name in table:
                opts.ctx.console.warn(f"dependency {name} already exists, ignoring...")
                continue
            table[name] = dep
        opts.source.save_to_disk()


def get_deps_table_for(manifest: Manifest, table: NewDependencyTable) -> Dependencies:
    match table:
        case NewDependencyTable.Deps:
            return manifest.dependencies
        case NewDependencyTable.Dev:
            return manifest.dev_dependencies
        case _:
            raise QuackPackError("we missmatched")


def make_deps(cli_input: list[str], type_: NewDependencyType) -> list[NewDependencySource]:
    result: list[NewDependencySource] = []
    logger.debug(f"making new deps of type {type_!s}")
    for name in cli_input:
        result.append(make_dep_impl(name, type_))
    return result


def make_dep_impl(name: str, type_: NewDependencyType) -> NewDependencySource:
    # For git sources name is url.
    if type_ is NewDependencyType.Git:
        logger.debug(f"making git entry with url '{name}'")
        return GitSource(name)
    # For local sources name is path to the root.
    if type_ is NewDependencyType.Local:
        logger.debug(f"making local entry with root '{name}'")
        return LocalSource(Path(name))
    # The only remaining thing is registry source.
    assert type_ is NewDependencyType.Registry, "we missmatched with types"
    # Syntax: `name` or `name@version`.
    if "@" not in name:
        logger.debug(f"making simple registry entry with name {name}")
        return RegistrySource(name, FetchVersion.Tag)
    name, version = name.split(sep="@", maxsplit=1)
    logger.debug(f"making detailed registry entry with name {name} and version {version}")
    return RegistrySource(name, version)


def check_if_has_flags(manifest: Manifest, flags: NewFlags):
    for flag in flags.global_:
        if flag not in manifest.features:
            raise QuackPackError(f"dependency {manifest.metadata.name} doesn't have flag {flag}")
    if manifest.metadata.name not in flags.per_package:
        return
    specific = flags.per_package[manifest.metadata.name]
    for flag in specific:
        if flag not in manifest.features:
            raise QuackPackError(f"dependency {manifest.metadata.name} doesn't have flag {flag}")


def print_add_info(
    console: Console,
    name: Identifier,
    enabled_flags: Iterable[Identifier],
    disabled_flags: Iterable[Identifier],
):
    console.info(f"Added new package {name!s}")
    # Package has no flags.
    if not enabled_flags and not disabled_flags:
        return
    console.info("Features:")
    for flag in enabled_flags:
        console.info(f"    [bold green]+[/] {flag!s}")
    for flag in disabled_flags:
        console.info(f"    [bold red]-[/] {flag!s}")

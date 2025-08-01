import asyncio
from dataclasses import dataclass

from quackpack.fetcher import FetcherContext
from quackpack.global_context import GlobalContext
from quackpack.manifest.summary import Summary
from quackpack.util.types.version import Version


@dataclass(frozen=True, kw_only=True)
class InfoOptions:
    ctx: GlobalContext

    package_name: str
    """
    Name of the package to look up.
    """

    package_version: Version | None
    """
    Specific version of the package to look up. If not provided, the latest version will be used.
    """


@dataclass(frozen=True, kw_only=True)
class InfoResult:
    name: str
    summary: Summary


def info(opts: InfoOptions) -> None:
    result = asyncio.run(_info_impl(opts.ctx, opts.package_name, opts.package_version))
    _print_collected_info(opts, result)


async def _info_impl(
    ctx: GlobalContext, package_name: str, package_version: Version | None
) -> InfoResult | None:
    with FetcherContext(ctx) as fetcher:
        result = await fetcher.search(ctx.registry_url(), package_name)

    exact_matches = [pkg for pkg in result.result if str(pkg.id).lower() == package_name.lower()]

    if package_version is not None:
        exact_matches = [
            pkg for pkg in exact_matches if Version.create_from_string(pkg.version) == package_version
        ]

    exact_matches.sort(key=lambda pkg: pkg.version, reverse=True)

    result = exact_matches[0] if exact_matches else None
    if result is None:
        return None

    with FetcherContext(ctx) as fetcher:
        metadata = await fetcher.get_package_metadata(ctx.registry_url(), result)
        if metadata.result is None:
            return None

    parsed_metadata = Summary.from_schema(metadata.result)

    return InfoResult(name=str(parsed_metadata.name), summary=parsed_metadata)


def _print_collected_info(opts: InfoOptions, info: InfoResult | None):
    console = opts.ctx.console
    package_name, package_version = opts.package_name, opts.package_version
    query = f"{package_name} {package_version}"

    if info is None:
        console.info(f"Didn't find any match for `{query}`")
        return

    console.info(f"Found info for `{query}:")
    console.print(f"[bold]Name:[/bold] {info.name}")
    console.print(f"[bold]Version:[/bold] {info.summary.version!s}")
    console.print(f"[bold]Description:[/bold] {info.summary.description or 'N/A'}")
    console.print(
        f"[bold]Authors:[/bold] {', '.join(info.summary.authors) if info.summary.authors else 'N/A'}"
    )
    console.print(f"[bold]License:[/bold] {info.summary.license or 'N/A'}")
    if info.summary.features:
        console.print("[bold]Features:[/bold]")
        for feature in info.summary.features:
            console.print(f"- {feature!s}")

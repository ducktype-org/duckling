import asyncio
from dataclasses import dataclass

from quackpack.fetcher import FetcherContext
from quackpack.global_context import GlobalContext


@dataclass(frozen=True, kw_only=True)
class SearchOptions:
    ctx: GlobalContext

    query: str
    """
    Query to be sent
    """


def search(opts: SearchOptions) -> None:
    matches = asyncio.run(_search_impl(opts.ctx, opts.query))
    _print_found_pkgs(opts, matches)


async def _search_impl(ctx: GlobalContext, query: str) -> list[str]:
    with FetcherContext(ctx) as fetcher:
        result = await fetcher.search(ctx.registry_url(), query)

    return list({str(pkg.id) for pkg in result.result})


def _print_found_pkgs(opts: SearchOptions, matches: list[str]):
    console = opts.ctx.console
    query = opts.query

    if not matches:
        console.info(f"Didn't find any packages for `{query}`")
        return

    console.info(f"Found packges for `{query}:")
    for pkg in matches:
        console.print(f"  - {pkg}")

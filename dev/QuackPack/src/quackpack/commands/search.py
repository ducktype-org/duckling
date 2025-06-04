import asyncio
from dataclasses import dataclass

from quackpack.fetcher import Fetcher
from quackpack.util.global_context import GlobalContext


@dataclass(frozen=True, kw_only=True)
class SearchOptions:
    ctx: GlobalContext

    query: str
    """
    Query to be sent
    """


def search(opts: SearchOptions) -> list[str]:
    return asyncio.run(_search_impl(opts.ctx, opts.query))


# TODO: FIXME: change to real address defined somewhere in the ctx...
async def _search_impl(ctx: GlobalContext, query: str) -> list[str]:
    with Fetcher(ctx) as fetcher:
        result = await fetcher.search(ctx.configuration.repository.url, query)

    return list({str(pkg.id) for pkg in result.result})

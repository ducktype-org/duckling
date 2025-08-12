import asyncio
from dataclasses import dataclass

from quackpack.core.fetcher import FetcherContext
from quackpack.core.types.package import Package
from quackpack.util.global_context import GlobalContext


@dataclass(frozen=True, kw_only=True)
class PublishOptions:
    ctx: GlobalContext

    source: Package
    """
    package to be published.
    """


def publish(opts: PublishOptions) -> None:
    asyncio.run(_publish_impl(opts.ctx, opts.source))


async def _publish_impl(ctx: GlobalContext, package: Package) -> None:
    with FetcherContext(ctx) as fetcher:
        await fetcher.publish_package(ctx.registry_url(), package)

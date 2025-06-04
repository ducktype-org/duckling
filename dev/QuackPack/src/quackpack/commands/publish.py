import asyncio
from dataclasses import dataclass

from quackpack.fetcher import Fetcher
from quackpack.project import Project
from quackpack.util.global_context import GlobalContext


@dataclass(frozen=True, kw_only=True)
class PublishOptions:
    ctx: GlobalContext

    source: Project
    """
    Project to be published.
    """


def publish(opts: PublishOptions) -> None:
    asyncio.run(_publish_impl(opts.ctx, opts.source))


# TODO: FIXME: change to real address defined somewhere in the ctx...
async def _publish_impl(ctx: GlobalContext, project: Project) -> None:
    with Fetcher(ctx) as fetcher:
        await fetcher.publish_package(ctx.configuration.repository.url, project)

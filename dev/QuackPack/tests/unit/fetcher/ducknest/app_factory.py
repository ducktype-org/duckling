import asyncio
import contextlib
from types import TracebackType
from typing import Self

from fastapi import APIRouter, FastAPI, Response
from fastapi.exceptions import HTTPException
from hypercorn.asyncio import serve  # pyright: ignore[reportUnknownVariableType]
from hypercorn.config import Config


class FastAPIFactory:
    def __init__(self, hypercorn_config: Config) -> None:
        self.app = None
        self.server_task = None
        self.hypercorn_config = hypercorn_config

    def create_app(self, router: APIRouter) -> FastAPI:
        self.app = FastAPI()
        self.app.include_router(router)

        @self.app.exception_handler(HTTPException)
        async def http_exception_handler(_request, exc):  # pyright: ignore[reportMissingParameterType, reportUnknownParameterType, reportUnusedFunction]  # noqa: RUF029
            return Response(status_code=exc.status_code)  # pyright: ignore[reportUnknownMemberType, reportUnknownArgumentType]

        return self.app

    async def start_server(self) -> None:
        self.server_task = asyncio.create_task(serve(self.app, self.hypercorn_config))  # pyright: ignore[reportArgumentType]

    async def stop_server(self) -> None:
        if self.server_task:
            self.server_task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self.server_task
            self.server_task = None

    async def __aenter__(self) -> Self:
        await self.start_server()
        return self

    async def __aexit__(
        self,
        exc_type: type[BaseException] | None,
        exc_val: BaseException | None,
        exc_tb: TracebackType | None,
    ) -> bool | None:
        await self.stop_server()
        return False if exc_type is not None else None

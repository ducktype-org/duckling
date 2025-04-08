import asyncio

from fastapi import FastAPI
from hypercorn.asyncio import serve  # pyright: ignore[reportUnknownVariableType]
from hypercorn.config import Config

from .routers import packages_routers


# TODO: it does not work with relative imports, fix if needed
def run() -> None:
    app = FastAPI()
    app.include_router(packages_routers.router)

    config = Config()
    config.bind = ["localhost:9001"]

    asyncio.run(serve(app, config))  # pyright: ignore[reportArgumentType]


if __name__ == "__main__":
    run()

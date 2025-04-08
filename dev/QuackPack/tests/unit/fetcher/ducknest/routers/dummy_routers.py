from fastapi import APIRouter
from fastapi.responses import JSONResponse, PlainTextResponse

from .handlers.dummy_handlers import Item, echo_handler, hello_world_handler, nap_handler

router = APIRouter(prefix="/dummy")


@router.get("/hello")
async def hello() -> JSONResponse:
    message = hello_world_handler()
    return JSONResponse(content={"msg": message})


@router.post("/echo")
async def echo(item: Item) -> PlainTextResponse:
    message = echo_handler(item)
    return PlainTextResponse(content=message)


@router.get("/nap/{secs}")
async def nap(secs: int) -> PlainTextResponse:
    message = await nap_handler(secs)
    return PlainTextResponse(content=str(message))

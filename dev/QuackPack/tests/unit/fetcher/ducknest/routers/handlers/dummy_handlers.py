import asyncio

from pydantic import BaseModel


class Item(BaseModel):
    name: str


def hello_world_handler() -> str:
    return "Hello World"


def echo_handler(item: Item) -> str:
    return item.name


async def nap_handler(secs: int) -> int:
    await asyncio.sleep(secs)
    return secs

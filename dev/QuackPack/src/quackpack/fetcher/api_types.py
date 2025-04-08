from pydantic import BaseModel

from quackpack.config.project.models import Configuration

__all__ = ["MultiMetadata", "Package", "SingleMetadata"]

SingleMetadata = Configuration


class MultiMetadata(BaseModel):
    packages_metadata: list[SingleMetadata]


class Package(BaseModel):
    name: str
    # TODO: find something better
    version: str | None = None

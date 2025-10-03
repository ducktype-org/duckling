from __future__ import annotations

from quackpack.core.types.manifest.schemas.manifest import ManifestSchema
from quackpack.util.global_context import GlobalContext

from .summary import Summary


class Manifest:
    def __init__(
        self, original_content: str, original_schema: ManifestSchema, summary: Summary, warnings: list[str]
    ):
        self._original_content = original_content
        # self._original_document = original_document
        self._original_schema = original_schema

        self._summary = summary

        self._warnings = warnings

    @property
    def original_content(self) -> str:
        return self._original_content

    # def original_document(self) -> TOMLDocument:
    #     return self._original_document

    @property
    def summary(self) -> Summary:
        return self._summary

    def emit_warnings(self, ctx: GlobalContext) -> None:
        for warning in self._warnings:
            ctx.error_console.warn(warning)

    @property
    def original_schema(self) -> ManifestSchema:
        return self._original_schema

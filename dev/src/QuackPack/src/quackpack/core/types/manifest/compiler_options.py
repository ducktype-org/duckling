from dataclasses import dataclass

from pydantic import RootModel

from quackpack.core.types.manifest.schemas.registry import (
    CompilerOptionsSchema,
    ProfileSchema,
)


@dataclass(frozen=True, kw_only=True)
class ManifestExtraOpts:
    compiler_flags: list[str]

    def into_schema(self) -> CompilerOptionsSchema:
        return CompilerOptionsSchema(compiler_flags=self.compiler_flags)

    @classmethod
    def from_schema(cls, schema: CompilerOptionsSchema) -> ManifestExtraOpts:
        return ManifestExtraOpts(compiler_flags=schema.compiler_flags)


class ProfileAndTargetCommon:
    def __init__(self, impl: dict[str, ManifestExtraOpts]):
        self._impl = impl

    def get_compiler_opts_for(self, key: str) -> list[str]:
        if key not in self._impl:
            return []
        return self._impl[key].compiler_flags

    def into_schema(self) -> ProfileSchema:
        return RootModel(
            {key: value.into_schema() for key, value in self._impl.items()}
        )


class Profiles(ProfileAndTargetCommon):
    def __init__(self, profiles: dict[str, ManifestExtraOpts]):
        super().__init__(profiles)

    @classmethod
    def from_schema(cls, schema: ProfileSchema) -> Profiles:
        return Profiles(
            {
                key: ManifestExtraOpts.from_schema(value)
                for key, value in schema.root.items()
            }
        )


class Targets(ProfileAndTargetCommon):
    def __init__(self, targets: dict[str, ManifestExtraOpts]):
        super().__init__(targets)

    @classmethod
    def from_schema(cls, schema: ProfileSchema) -> Targets:
        return Targets(
            {
                key: ManifestExtraOpts.from_schema(value)
                for key, value in schema.root.items()
            }
        )

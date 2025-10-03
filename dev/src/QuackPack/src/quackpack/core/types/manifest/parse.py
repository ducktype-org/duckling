from pathlib import Path

from pydantic import BaseModel

from quackpack.core.types.manifest.schemas.manifest import (
    DependencyConditionSchema,
    DependencySchema,
    DetailedFeatureSchema,
    FeaturesSchema,
    ManifestSchema,
    MetadataSchema,
    OredSemverSchema,
    ProfileSchema,
    SemverSchema,
    SimpleSourceSchema,
    SourceSchema,
    VersionSchema,
)
from quackpack.util.duckling_compatibility import is_valid_identifier
from quackpack.util.global_context import GlobalContext
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version
from quackpack.util.yaml.strict_parsing import ConfigFileLoadError, load_and_validate

from .compiler_options import ManifestExtraOpts, Profiles, Targets
from .dependencies import Dependencies
from .dependency import Conditions, Dependency, DependencyFeature
from .dependency_spec import DependencySpec
from .features import Features
from .manifest import Manifest
from .root_spec import RootSpec
from .source import GitSource, LocalSource, RegistrySource, Source
from .summary import Summary

logger = get_logger(__name__)


class Scope:
    def __init__(self):
        self._stack: list[str] = []

    def push(self, name: str) -> None:
        self._stack.append(name)

    def pop(self) -> str:
        return self._stack.pop()

    def format(self) -> str:
        assert self._stack, "format() called with empty stack"
        return ".".join(self._stack)


def parse_manifest(source: Path, ctx: GlobalContext) -> Manifest:
    source = source.expanduser().resolve()
    logger.debug(f"parsing manifest from `{source}`")
    try:
        return parse_manifest_impl(source, ctx)
    except Exception as e:
        if isinstance(e, ConfigFileLoadError):
            e = QuackPackError(e)
        e.add_note(f"when parsing user manifest at {source!s}")
        raise e from None


def parse_manifest_impl(source: Path, ctx: GlobalContext) -> Manifest:
    content = source.read_text()
    schema = create_schema(source)
    package_root = source.parent
    logger.debug(f"presumed package root is `{package_root!s}`")
    summary = summary_from_schema(schema, package_root, ctx)
    warnings = create_warnings(schema)
    return Manifest(content, schema, summary, warnings)


def create_schema(source: Path) -> ManifestSchema:
    return load_and_validate(source, ManifestSchema)


def validate_basic_schema(schema: ManifestSchema) -> None:
    if schema.metadata is None:
        raise QuackPackError("missing obligatory section `metadata`")
    if schema.metadata.version is None:
        raise QuackPackError("missing obligatory key `metadata.version`")
    if schema.metadata.name is None:
        raise QuackPackError("missing obligatory key `metadata.name`")
    if not is_valid_identifier(schema.metadata.name):
        raise QuackPackError(
            f"package `{schema.metadata.name}` from `metadata.name` is not a valid identifier"
        )


def summary_from_schema(
    schema: ManifestSchema, package_root: Path, ctx: GlobalContext
) -> Summary:
    validate_basic_schema(schema)
    assert schema.metadata is not None, "by validate_basic_schema"

    context_stack = Scope()
    root_spec = parse_root_package_spec(schema.metadata)

    context_stack.push("dependencies")
    deps = parse_dependencies(schema.dependencies, package_root, context_stack, ctx)
    context_stack.pop()

    context_stack.push("dev_dependencies")
    dev_deps = parse_dependencies(
        schema.dev_dependencies, package_root, context_stack, ctx
    )
    context_stack.pop()

    context_stack.push("features")
    features = parse_features(schema.features, context_stack)
    context_stack.pop()

    profiles = parse_profile(schema.profiles)
    targets = parse_targets(schema.targets)

    return Summary(
        root_spec,
        features,
        schema.metadata.authors,
        schema.metadata.license,
        schema.metadata.description,
        deps,
        dev_deps,
        profiles,
        targets,
    )


def parse_root_package_spec(schema: MetadataSchema) -> RootSpec:
    assert schema.name is not None, "by validate_basic_schema"
    assert schema.version is not None, "by validate_basic_schema"
    logger.debug(f"package name is `{schema.name}`, version is `{schema.version.root}`")
    name = Identifier(schema.name)
    version = Version.create_from_string(schema.version.root)
    return RootSpec(name, version)


def parse_version(source: VersionSchema | OredSemverSchema) -> list[Version]:
    if isinstance(source, (SemverSchema, list)):
        if isinstance(source, SemverSchema):
            return [Version.create_from_string(source.root)]
        return [Version.create_from_string(x.root) for x in source]
    assert isinstance(source, OredSemverSchema), "by schemas"
    return [Version.create_from_string(x) for x in source.split_versions()]


def parse_dependencies(
    schema: dict[str, DependencySchema] | None,
    package_root: Path,
    context_stack: Scope,
    ctx: GlobalContext,
) -> Dependencies:
    if schema is None:
        logger.debug("empty deps or dev_deps")
        return Dependencies({})
    result: dict[Identifier, Dependency] = {}
    for name, dep_schema in schema.items():
        context_stack.push(name)
        if not is_valid_identifier(name):
            raise QuackPackError(
                f"dependency `{context_stack.format()}` is not a valid identifier"
            )
        manifest_name = Identifier(name)
        result[manifest_name] = parse_single_dependency(
            manifest_name, dep_schema, package_root, context_stack, ctx
        )
        context_stack.pop()
    return Dependencies(result)


def parse_single_dependency(
    manifest_name: Identifier,
    schema: DependencySchema,
    package_root: Path,
    context_stack: Scope,
    ctx: GlobalContext,
) -> Dependency:
    logger.debug(f"parsing dependency `{manifest_name!s}`")
    context_stack.push("source")
    source = parse_source_from_schema(schema, package_root, context_stack, ctx)
    context_stack.pop()

    versions = parse_dep_version(schema)
    spec = DependencySpec(manifest_name, versions, source)

    if not versions and source.is_registry():
        raise QuackPackError(
            f"dependency `{context_stack.format()}` is a registry dependency, but does not provide `{context_stack.format()}.version` field"
        )

    context_stack.push("features")
    features = parse_dep_features(schema.features, context_stack)
    context_stack.pop()

    context_stack.push("pinned")
    is_pinned = parse_is_pinned(schema, source, context_stack)
    context_stack.pop()

    if is_pinned and len(versions) != 1:
        raise QuackPackError(
            f"dependency `{context_stack.format()}` is pinned, but contains multiple possible versions"
        )

    context_stack.push("conditions")
    conds = (
        parse_conditions(schema.conditions, context_stack)
        if schema.conditions is not None
        else None
    )
    context_stack.pop()

    real_name = parse_real_name(schema.source, context_stack)

    return Dependency(spec, features, is_pinned, conds, real_name)


def parse_source_from_schema(
    schema: DependencySchema,
    package_root: Path,
    context_stack: Scope,
    ctx: GlobalContext,
) -> Source:
    if schema.source is None:
        logger.debug("missing source, falling back to default registry?")
        if has_version(schema):
            return RegistrySource(registry_url=ctx.registry_url())
        context_stack.pop()  # So we don't print "source" below in format().
        raise QuackPackError(
            f"couldn't determine source of dependency {context_stack.format()}\nhint: provide `version` or `source` field"
        )
    source = schema.source
    if has_version(schema) and has_local_source(source):
        context_stack.pop()  # So we don't print "source" below in format().
        raise QuackPackError(
            f"can't determine type of dependency `{context_stack.format()}`, please remove one of fields `{context_stack.format()}.version` or `{context_stack.format()}.source.path`"
        )
    match (
        has_registry_source(source),
        has_local_source(source),
        has_git_source(source),
    ):
        case (False, False, False):
            logger.debug("didn't find any source")
            check_no_git(source, context_stack)
            check_no_local(source, context_stack)
            check_no_registry(source, context_stack)
            if has_version(schema):
                logger.debug("...but has version, assuming registry source")
                return RegistrySource(registry_url=ctx.registry_url())
            context_stack.pop()
            raise QuackPackError(
                f"couldn't determine source of dependency {context_stack.format()}\nhint: provide `version` or `source` fields"
            )
        case (True, False, False):
            logger.debug("found registry source")
            check_no_git(source, context_stack)
            check_no_local(source, context_stack)
            # str means another registry
            if isinstance(source, str):
                logger.debug(
                    "source is a string, assuming different registry url=`{source}`"
                )
                return RegistrySource(registry_url=source)
            url = (
                source.registry_url
                if source.registry_url is not None
                else ctx.registry_url()
            )
            return RegistrySource(registry_url=url)
        case (False, True, False):
            logger.debug("found local path source")
            check_no_git(source, context_stack)
            check_no_registry(source, context_stack)
            assert isinstance(source, SourceSchema), "by match"
            assert source.path is not None, "by match"
            logger.debug(f"manifest path is `{source.path!s}`")
            provided_path = source.path.expanduser()
            logger.debug(f"after expanding tildes it's `{provided_path!s}`")
            if provided_path.is_absolute():
                dir_root = provided_path
                logger.debug("it's absolute, using it")
            else:
                dir_root = package_root / provided_path
                dir_root = dir_root.resolve()
                logger.debug(f"it was not absolute, guessed `{dir_root!s}`")
            return LocalSource(
                absolute_dir_root=dir_root, dir_entry_in_manifest=source.path
            )
        case (False, False, True):
            logger.debug("found git source")
            check_no_local(source, context_stack)
            check_no_registry(source, context_stack)
            check_exclusive_git_fields(source, context_stack)
            assert isinstance(source, SourceSchema), "by match"
            assert source.git_url is not None, "by match"
            logger.debug(
                f"url=`{source.git_url}`, branch=`{source.branch}`, commit=`{source.commit}`, tag=`{source.tag}`"
            )
            return GitSource(
                git_url=source.git_url,
                branch=source.branch,
                commit=source.commit,
                tag=source.tag,
            )
        case (True, False, True):
            context_stack.pop()
            raise QuackPackError(
                f"couldn't determine source of dependency {context_stack.format()}\nhint: remove `source.git_url` or `source.registry_url`"
            )
        case (True, True, False):
            context_stack.pop()
            raise QuackPackError(
                f"couldn't determine source of dependency {context_stack.format()}\nhint: remove `source.path` or `source.registry_url`"
            )
        case (False, True, True):
            context_stack.pop()
            raise QuackPackError(
                f"couldn't determine source of dependency {context_stack.format()}\nhint: remove `source.path` or `source.git_url`"
            )
        case (True, True, True):
            context_stack.pop()
            raise QuackPackError(
                f"couldn't determine source of dependency {context_stack.format()}\nhint: leave only one of fields `source.registry_url`, `source.path` or `source.git_url`"
            )
        case _:
            assert False, "unhandled case in source_from_schema"


def parse_real_name(
    source: SourceSchema | SimpleSourceSchema | None, context_stack: Scope
) -> Identifier | None:
    if source is None:
        logger.debug("package is not an alias")
        return None
    if isinstance(source, str):
        logger.debug("source is simple schema, without alias")
        return None
    if source.name is None:
        logger.debug("source doesn't have an alias")
        return None
    logger.debug(f"assuming alias `{source.name}`")
    if not is_valid_identifier(source.name):
        raise QuackPackError(
            f"dependency `{context_stack.format()}` is an alias for `{source.name}`, but latter is not a valid identifier"
        )
    return Identifier(source.name)


def parse_dep_version(schema: DependencySchema) -> list[Version]:
    # NOTE: Lokalne zależności mają pustą listę.
    if schema.version is None:
        return []
    return parse_version(schema.version)


def has_version(schema: DependencySchema) -> bool:
    return schema.version is not None


def check_no_git(source: SourceSchema | SimpleSourceSchema, context_stack: Scope):
    if isinstance(source, str):
        return
    fields = (
        (source.git_url, "git_url"),
        (source.tag, "tag"),
        (source.branch, "branch"),
        (source.commit, "commit"),
    )
    for field, name in fields:
        if field is not None:
            old = context_stack.pop()
            raise QuackPackError(
                f"expected dependency `{context_stack.format()}` to not be a git dependency, but field `{context_stack.format()}.{old}.{name}` is set"
            )


def check_exclusive_git_fields(
    source: SourceSchema | SimpleSourceSchema, context_stack: Scope
):
    if isinstance(source, str):
        return
    exclusive_fields = ((source.tag, "tag"), (source.branch, "branch"))
    present_fields_by_name = [x[1] for x in exclusive_fields if x[0] is not None]
    if len(present_fields_by_name) <= 1:
        return
    formatted = (f"`{context_stack.format()}.{x}`" for x in present_fields_by_name)
    formatted = ", ".join(formatted)
    raise QuackPackError(
        f"dependency `{context_stack.format()}` is a git dependency, but contains mutually exclusive fields: {formatted}"
    )


def check_no_local(source: SourceSchema | SimpleSourceSchema, context_stack: Scope):
    if isinstance(source, str):
        return
    fields = ((source.path, "path"),)
    for field, name in fields:
        if field is not None:
            old = context_stack.pop()
            raise QuackPackError(
                f"expected dependency `{context_stack.format()}` to not be a local dependency, but field `{context_stack.format()}.{old}.{name}` is set"
            )


def check_no_registry(source: SourceSchema | SimpleSourceSchema, context_stack: Scope):
    if isinstance(source, str):
        context_stack.pop()
        raise QuackPackError(
            f"expected dependency `{context_stack.format()}` to not be a registry dependency, but it specifies another registry source in `source`"
        )
    fields = ((source.registry_url, "registry_url"),)
    for field, name in fields:
        if field is not None:
            old = context_stack.pop()
            raise QuackPackError(
                f"expected dependency `{context_stack.format()}` to not be a registry dependency, but field `{context_stack.format()}.{old}.{name}` is set"
            )


def has_registry_source(source: SourceSchema | SimpleSourceSchema) -> bool:
    if isinstance(source, str):
        return True
    assert isinstance(source, SourceSchema), "by pydantic"
    return source.registry_url is not None or (
        # Mamy pole `name`, ale bez `git_url` czy `path`.
        source.name is not None
        and not has_git_source(source)
        and not has_local_source(source)
    )


def has_git_source(source: SourceSchema | SimpleSourceSchema) -> bool:
    if isinstance(source, str):
        return False
    assert isinstance(source, SourceSchema), "by pydantic"
    return source.git_url is not None


def has_local_source(source: SourceSchema | SimpleSourceSchema) -> bool:
    if isinstance(source, str):
        return False
    assert isinstance(source, SourceSchema), "by pydantic"
    return source.path is not None


def parse_dep_features(
    source: list[str | DetailedFeatureSchema] | None, context_stack: Scope
) -> list[DependencyFeature]:
    if source is None:
        return []
    result: list[DependencyFeature] = []
    for feature in source:
        if isinstance(feature, str):
            context_stack.push(feature)
            if not is_valid_identifier(feature):
                raise QuackPackError(
                    f"feature `{context_stack.format()}` is not a valid identifier"
                )
            result.append(DependencyFeature(Identifier(feature), None))
            context_stack.pop()
            continue
        # Should we support one dictionary with multiple keys or multiple dictionaries with one key per dictionary, or does it really matter?
        for name, conds in feature.root.items():
            context_stack.push(name)
            if not is_valid_identifier(name):
                raise QuackPackError(
                    f"feature `{context_stack.format()}` is not a valid identifier"
                )
            name_as_ident = Identifier(name)

            context_stack.push("conditions")
            result.append(
                DependencyFeature(name_as_ident, parse_conditions(conds, context_stack))
            )
            context_stack.pop()

            context_stack.pop()
    return result


def parse_conditions(
    conds: DependencyConditionSchema, context_stack: Scope
) -> Conditions:
    context_stack.push("system")
    system = conds.system
    if system is not None and not system:
        raise QuackPackError(f"`{context_stack.format()}` is an empty list")
    context_stack.pop()

    context_stack.push("arch")
    cpu = conds.arch
    if cpu is not None and not cpu:
        raise QuackPackError(f"`{context_stack.format()}` is an empty list")
    context_stack.pop()

    context_stack.push("package_features")
    features = parse_list_of_idents_or_none(
        conds.package_features, context_stack, allow_empty=False
    )
    context_stack.pop()

    return Conditions(system, cpu, features)


def parse_list_of_idents(
    source: list[str], context_stack: Scope, *, allow_empty: bool = True
) -> list[Identifier]:
    result: list[Identifier] = []
    for f in source:
        context_stack.push(f)
        if not is_valid_identifier(f):
            raise QuackPackError(
                f"`{context_stack.format()}` is not a valid identifier"
            )
        result.append(Identifier(f))
        context_stack.pop()
    if not result and not allow_empty:
        raise QuackPackError(f"`{context_stack.format()}` is an empty list")
    return result


def parse_list_of_idents_or_none(
    source: list[str] | None, context_stack: Scope, *, allow_empty: bool = True
) -> list[Identifier] | None:
    if source is None:
        return None
    return parse_list_of_idents(source, context_stack, allow_empty=allow_empty)


def parse_is_pinned(
    schema: DependencySchema, source: Source, context_stack: Scope
) -> bool:
    if schema.pinned is not None and not source.is_registry():
        raise QuackPackError(
            f"field `{context_stack.format()}` is only allowed for registry dependencies"
        )
    if schema.pinned is None:
        return False
    return schema.pinned


def parse_features(features: FeaturesSchema | None, context_stack: Scope) -> Features:
    if features is None:
        return Features({})
    impl: dict[Identifier, list[Identifier]] = {}
    for feature, pointers in features.root.items():
        context_stack.push(feature)
        if not is_valid_identifier(feature):
            raise QuackPackError(
                f"feature `{context_stack.format()}` is not a valid identifier"
            )
        impl[Identifier(feature)] = parse_list_of_idents(pointers, context_stack)
        context_stack.pop()
    return Features(impl)


def parse_compiler_opts(opts: ProfileSchema) -> dict[str, ManifestExtraOpts]:
    result: dict[str, ManifestExtraOpts] = {}
    for name, value in opts.root.items():
        flags = value.compiler_flags or []
        result[name] = ManifestExtraOpts(compiler_flags=flags)
    return result


def parse_profile(schema: ProfileSchema | None) -> Profiles:
    if schema is None:
        return Profiles({})
    return Profiles(parse_compiler_opts(schema))


def parse_targets(schema: ProfileSchema | None) -> Targets:
    if schema is None:
        return Targets({})
    return Targets(parse_compiler_opts(schema))


def create_warnings(schema: ManifestSchema) -> list[str]:
    result: list[str] = []
    populate_unused_key_warnings(schema, Scope(), result)
    return result


def populate_unused_key_warnings(
    schema: BaseModel, context_stack: Scope, result: list[str]
) -> None:
    to_iter = schema.__pydantic_extra__ or {}
    for key in to_iter:
        context_stack.push(key)
        result.append(f"unused manifest key `{context_stack.format()}`")
        context_stack.pop()
    for key, value in iter(schema):
        if isinstance(value, BaseModel):
            context_stack.push(key)
            populate_unused_key_warnings(value, context_stack, result)
            context_stack.pop()

from dataclasses import dataclass, field
from pathlib import Path

from rich.prompt import Confirm, Prompt
from tomlkit import TOMLDocument, dumps  # pyright: ignore[reportUnknownVariableType]

from quackpack.core.package_loader import PackageLoader
from quackpack.core.types.manifest.editable import EditableManifest
from quackpack.core.types.manifest.schemas.manifest import ManifestSchema
from quackpack.driver.commands.init.types import InitOptions
from quackpack.util.duckling_compatibility import is_valid_identifier
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError

logger = get_logger(__name__)


@dataclass(frozen=True, kw_only=True)
class FileWithData:
    destination: Path
    data: str

    def execute_impl(self) -> None:
        self.destination.parent.mkdir(parents=True, exist_ok=True)
        logger.debug(f"creating new file `{self.destination}` for package")
        self.destination.write_text(self.data)

    def execute(self) -> None:
        self.execute_impl()

    def __post_init__(self) -> None:
        if not self.destination.is_relative_to(Path.cwd()):
            raise ValueError(f"path `{self.destination}` is not relative to CWD")


@dataclass(frozen=True, kw_only=True)
class PlainDirectory:
    destination: Path

    def execute_impl(self) -> None:
        logger.debug(f"creating new directory `{self.destination}` for package")
        self.destination.mkdir(parents=True, exist_ok=True)

    def execute(self) -> None:
        self.execute_impl()

    def __post_init__(self) -> None:
        if not self.destination.is_relative_to(Path.cwd()):
            raise ValueError(f"path `{self.destination}` is not relative to CWD")


@dataclass(frozen=True, kw_only=True)
class VenvConfigCreator:
    options: InitOptions

    def execute_impl(self) -> None:
        to_create: list[FileWithData | PlainDirectory] = []
        venv_config_toml = TOMLDocument()

        if self.options.is_ephemeral:
            venv_config_toml["ephemeral"] = self.options.is_ephemeral

        if self.options.use_local_storage:
            local_storage_path = self.options.destination / PackageLoader.LOCAL_STORAGE_NAME

            venv_config_toml["local_storage"] = self.options.use_local_storage
            local_storage_dir = PlainDirectory(destination=local_storage_path)

            to_create.append(local_storage_dir)

        if self.options.expose_freezefile:
            venv_config_toml["expose_freezefile"] = self.options.expose_freezefile

        if venv_config_toml:
            venv_config_path = self.options.destination / PackageLoader.VENV_CONFIG_NAME
            venv_config_file = FileWithData(destination=venv_config_path, data=dumps(venv_config_toml))

            to_create.append(venv_config_file)

        for x in to_create:
            x.execute()

    def execute(self) -> None:
        self.execute_impl()


@dataclass(frozen=True, kw_only=True)
class BasicPackage:
    options: InitOptions

    @property
    def file(self) -> FileWithData:
        hello_world_path = self.options.destination / "src" / "main.duck"

        return FileWithData(
            destination=hello_world_path,
            data=r"""print("Hello World!\n")
""",
        )

    def execute_impl(self) -> None:
        self.file.execute()
        VenvConfigCreator(options=self.options).execute()

        # Should we also initialize git repo?

    def execute(self) -> None:
        self.execute_impl()
        self.options.ctx.console.info(
            f"successfully created new package `{self.options.name}` at `{self.options.destination}`"
        )


@dataclass(frozen=True, kw_only=True)
class AdvancedPackage:
    options: InitOptions
    manifest: ManifestSchema
    creators: list[PlainDirectory | FileWithData] = field(default_factory=list[PlainDirectory | FileWithData])

    def execute_impl(self) -> None:
        assert self.manifest.metadata is not None, "it should be populated earlier"
        metadata = self.manifest.metadata
        console = self.options.ctx.console
        self.creators.append(BasicPackage(options=self.options).file)
        if Confirm.ask(prompt="Create `docs` directory?", default=True, console=console):
            self.creators.append(PlainDirectory(destination=self.options.destination / Path("docs")))
        if Confirm.ask(prompt="Create `tests` directory?", default=True, console=console):
            self.creators.append(PlainDirectory(destination=self.options.destination / Path("tests")))
        if name := Prompt.ask("Set package name", console=console):
            if not is_valid_identifier(name):
                raise QuackPackError(f"`{name}` is not a valid package name")
            metadata.name = name
        metadata.authors = [Prompt.ask("Set package author", console=console)]
        for creator in self.creators:
            creator.execute_impl()
        # NOTE: We CWD'ed.
        PackageLoader.MANIFEST_NAME.write_text(
            EditableManifest.create_with_data(
                self.manifest.model_dump(exclude_none=True, exclude_unset=True)
            ).as_str()
        )
        VenvConfigCreator(options=self.options).execute()

    def execute(self) -> None:
        self.execute_impl()
        assert self.manifest.metadata is not None, "it should be populated earlier"
        metadata = self.manifest.metadata
        console = self.options.ctx.console
        console.info(f"successfully created new package `{metadata.name}` at `{self.options.destination}`")

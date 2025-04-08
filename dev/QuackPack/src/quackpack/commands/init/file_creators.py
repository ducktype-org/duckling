from dataclasses import dataclass, field
from pathlib import Path

from rich.prompt import Confirm, Prompt

from quackpack.commands.init.types import InitOptions
from quackpack.config.project import Venv
from quackpack.config.project.models import Configuration
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


@dataclass(frozen=True, kw_only=True)
class FileWithData:
    destination: Path
    data: str

    def execute(self) -> None:
        self.destination.parent.mkdir(parents=True, exist_ok=True)
        logger.debug(f"Creating new file '{self.destination}' for venv")
        with open(self.destination, "w") as f:
            f.write(self.data)

    def __post_init__(self) -> None:
        if not self.destination == self.destination.resolve().relative_to(Path.cwd()):
            raise ValueError(f"Path {self.destination} is not relative to CWD")


@dataclass(frozen=True, kw_only=True)
class PlainDirectory:
    destination: Path

    def execute(self) -> None:
        logger.debug(f"Creating new directory '{self.destination}' for venv")
        self.destination.mkdir(parents=True, exist_ok=True)

    def __post_init__(self) -> None:
        if not self.destination == self.destination.resolve().relative_to(Path.cwd()):
            raise ValueError(f"Path {self.destination} is not relative to CWD")


@dataclass(frozen=True, kw_only=True)
class BasicProject:
    options: InitOptions | None = None
    file: FileWithData = field(
        default_factory=lambda: FileWithData(
            destination=Path("src/main.duck"),
            data=r"""print("Hello World!\n")
""",
        )
    )

    def execute(self) -> None:
        self.file.execute()
        logger.debug("[bold red]FIXME[/]: Initialize git repo")
        if self.options is None:
            return
        self.options.ctx.console.info(
            f"Successfully created new project '{self.options.name}' at '{self.options.destination}'"
        )


@dataclass(frozen=True, kw_only=True)
class AdvancedProject:
    options: InitOptions
    project_config: Configuration
    creators: list[PlainDirectory | FileWithData] = field(default_factory=list)

    def execute(self) -> None:
        console = self.options.ctx.console
        self.creators.append(BasicProject().file)
        if Confirm.ask(prompt="Create docs directory?", default=True, console=console):
            self.creators.append(PlainDirectory(destination=Path("docs")))
        if Confirm.ask(prompt="Create tests directory?", default=True, console=console):
            self.creators.append(PlainDirectory(destination=Path("tests")))

        self.project_config.metadata.name = Prompt.ask(
            "Set project name", default=self.project_config.metadata.name, console=console
        )
        self.project_config.metadata.author = Prompt.ask("Set project author", console=console)

        for creator in self.creators:
            creator.execute()
        # NOTE: We CWD'ed.
        self.project_config.save_to(Venv.CONFIG_FILE_NAME)
        self.options.ctx.console.info(
            f"Successfully created new project '{self.project_config.metadata.name}' at '{self.options.destination}'"
        )

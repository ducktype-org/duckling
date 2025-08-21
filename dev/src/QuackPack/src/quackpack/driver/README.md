### quackpack.driver

Command-line interface and command implementations.

- **cli/**: argument parsing, console utilities and subcommands setup.
- **commands/**: implementation of user-facing commands (e.g., `build`, `add`, `run`, `search`, `sync`).

## `cli/console.py`
Wrapper around `rich.console.Console` with convenience helpers.

## `cli/setup_and_run.py`
Bootstraps CLI, environment, and dispatch.

## `commands/*.py`
Individual command handlers.

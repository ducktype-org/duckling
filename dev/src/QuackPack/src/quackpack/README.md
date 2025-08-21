### QuackPack package

Short overview of the package structure and key entry points.

### Top-level modules
- `compile/`: temporary compiler integration and compile-and-run bridge to `duckc`.
- `core/`: core domain logic (signals, storage, manifest/types, fetcher, solver).
- `driver/`: CLI, command dispatch and argument parsing.
- `util/`: shared utilities (logging, env, YAML parsing, file locks, types, etc.).

### Entry points

## `__main__.py`
Enables `python -m quackpack`; calls `quackpack.main()`.

## `__init__.py`
Configures robust signal handling and launches the CLI via `driver.cli.setup_and_run`.

### Most important files

## `core/signals.py`
Robust signal handling abstractions.

## `core/package_loader.py`
Discovers project roots and manifests (`quackconfig.yml`).

## `driver/cli/run.py`
User-facing CLI launcher.

## `driver/commands/*`
User-facing command implementations.

## `compile/compiler.py`
Glue layer to the `duckc` compiler (temporary implementation).

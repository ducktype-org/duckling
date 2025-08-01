# But what happens under the hood of Quack Pack?

On the very high level, Quack Pack code execution can be broken into three steps: performing some general setup, parsing arguments, and taking some appropriate action.

## General setup

The very first function, which is called, is [`src/quackpack/__init__.py:main`](../../src/quackpack/__init__.py).
It's responsible only for two things: overriding Python's ugly signal handlers for `SIGINT` and `SIGTERM`, and then calling [`src/quackpack/cli/setup_and_run.py`](../../src/quackpack/cli/setup_and_run.py).

`setup_and_run`:

- enables loggers and custom parser syntax highlighting,
- creates `Console`s objects, which are used for writing to `stdout` or `stderr`,
- catches exceptions,
- executes [`src/quackpack/cli/run.py`](../../src/quackpack/cli/run.py), which actually parses command-line.

## Parsing

All of the parsing is done in [`src/quackpack/cli/run.py`](../../src/quackpack/cli/run.py).
Before actually passing command-line arguments to the parser, we:
- try to fix possible user typos in subcommand names,
- expand user aliases.

## Subcommand execution

Then, on the very high level, we almost immediately call into [`src/quackpack/commands/`](../../src/quackpack/commands/) directory, where real execution begins.

More details can be found in [`adding_new_subcommand`](adding_new_subcommand.md) document, or by reading the code.

# Quack Pack

An 🚀 *extremely fast* 📦 package manager for 🦆 Duckling programming language, written in 🐍 Python.

## Features

* 🚀 Custom API over ~`HTTP/3` and `QUIC`~ `HTTP/2` for fastest network requests
* 🐢 Zero-cost venv abstraction for clean, easy to maintain development environments
* 📦 Shared caches for downloaded packages, compiled files, and more!
* 🐍 Written in beloved Python for easy maintenance and portability

## Building the project

### With `pip`

You can just run in virtual environment:

```bash
pip install .
```

### With `uv`

We are using `uv` as a modern package manager for Python and as our build system.

Check their [Installation guide](https://docs.astral.sh/uv/getting-started/installation/) for how to install `uv`.

To run Quack Pack all you need is:

```bash
uv run quackpack
```

## Running locally CI

We have two CI stages: test and lint.

All are managed by `uv`, and can be run locally before pushing.

### Test

```bash
uv run pytest
uv run pyright
```

### Lint

```bash
uv run ruff check
uv run ruff format
```

## Adding `pre-commit`

All hooks are currently set to run on `pre-push`, since we use squash-and-merge for merge requests.

```bash
pre-commit install --hook-type pre-push
```

`pre-commit` runs almost all CI steps (except `pytest`) before pushing.

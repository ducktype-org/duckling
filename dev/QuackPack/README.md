# Quack Pack

An 🚀 *extremely fast* 📦 package manager for 🦆 Duckling programming language written in 🐍 Python.

## Features

* 🚀 custom API over ~`HTTP/3` and `QUIC`~ `HTTP/2` for fastest network requests
* 🐢 zero-cost venv’s abstraction for clean and easy to maintain development environments
* 📦 shared caches for downloaded packages, compiled files and more!
* 🐍 written in beloved Python for ease of maintaining and porting

## Building the project

We are using `uv` as a modern package manager for Python and as a build system.

Check their [Installation guide](https://docs.astral.sh/uv/getting-started/installation/) on how to install `uv`.

To run Quack Pack all you need is:
```bash
cd quackpack
uv run
```

And building our self hosted packages server, Ducknest, is as simple as building Quack Pack.
```bash
cd ducknest
uv run
```

## Testing locally CI
We have three global stages of CI: linting, formatting, and tests.

First two are handled by [ruff](https://docs.astral.sh/ruff/):
```bash
ruff check
ruff format
```

and last by a combination pytest+uv:
```bash
# In appropriate subproject directory.
uv run pytest
```

## Adding pre-commit
All hooks are currently pre-push, since we use squash-and-merge for merge requests.
```bash
pre-commit install --hook-type pre-push
```

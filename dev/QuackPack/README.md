# Quack Pack

An 🚀 *extremely fast* 📦 package manager for 🦆 Duckling programming language, written in 🐍 Python.

## Features

* 🚀 Custom API over ~`HTTP/3` and `QUIC`~ `HTTP/2` for fastest network requests
* 🐢 Zero-cost venv abstraction for clean, easy to maintain development environments
* 📦 Shared caches for downloaded packages, compiled files, and more!
* 🐍 Written in beloved Python for easy maintenance and portability

## Building the project

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

## [`git-lfs`](https://git-lfs.com/) and `pre-commit` hooks

Git currently doesn't support multiple hooks of the same type.
Here's how to combine `git-lfs` hooks with `pre-commit`:

1. Install `git-lfs` as explained in their documentation.

1. Clone the repo. You should see many similar looking hooks (the ones without `.sample` extension) in `.git/hooks/` directory — those are from `git-lfs`.

1. Install `pre-commit` as explained above. It should warn you, that old hook has been moved to `.git/hooks/pre-push.legacy`.

1. Now do some manual file editing on `.git/hooks/pre-push`:

    1. Remove the `exec` command from any `if` branches.

    1. Copy the relevant lines from `.git/hooks/pre-push.legacy` (usually those starting with `command -v ...` and `git lfs <hook-type> ...`)
       and paste them at the end of the current file.

    1. [Optional, but recommended] Add `set -e` at the top of the file to ensure early exit on failure (for `bash`).

Now `pre-commit` and `git-lfs` should coexist peacefully.

### Troubleshooting

1. Make sure the hook type used by `git-lfs` matches the filename.
   At the end of the hook file you should see a line like: `git lfs <hook-type> ...`.

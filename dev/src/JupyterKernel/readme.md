# Duckling Jupyter kernel

A Jupyter kernel for Duckling notebooks (JupyterLab, Jupyter Notebook, VS Code). It drives
`duck repl -- --framed`, so cells can import:

- inside a package (the notebook lives in a directory with a `quackconfig.yaml`): the package
  itself and its dependencies;
- elsewhere: the global packages (`duck add -g ...`).

## Install

```bash
pip install ./src/JupyterKernel           # or `pip install -e` for development
python -m duckling_kernel install --user  # or --sys-prefix inside a virtualenv
```

`duck` and `duckc` must be on `PATH`. Environment variables:

| Variable | Effect |
|---|---|
| `DUCK_BIN` | Path of the `duck` binary to use. |
| `DUCKLING_KERNEL_COMMAND` | Full REPL command, e.g. `duckc repl --framed` (std only, no packages). |

Syntax highlighting: `pip install jupyterlab-duckling-syntax-highlighting`.

## Cells

- A cell is one REPL input: definitions persist, expressions print their result.
- Program output goes to the cell; compiler diagnostics go to stderr and mark the cell as failed.
- REPL commands work as cells (`/symbols`, `/reset`, ...).
- `Tab` completes keywords, types and names defined in the session.

## Rich output

A program shows rich output by printing display blocks:

```
\x1e@duck-display begin <mime type>
<payload lines>
\x1e@duck-display end
```

(`\x1e` is the ASCII record separator.) Blocks printed back to back form one output with several
MIME types, so a frontend picks the richest it supports, e.g. `text/html` with an
`image/svg+xml` and `text/plain` fallback.

## Framed REPL mode

`duckc repl --framed` is the REPL's interface for programs:

- each input ends with a line holding `\x1e@duck-end`, so inputs may span lines;
- `\x1e@duck-ready` is printed on stdout and on stderr whenever the REPL waits for input;
- expression results are printed as `\x1e@duck-result <value>`.

## Tests

```bash
cd src/JupyterKernel
python3 -m unittest discover -s tests -t .
DUCKLING_KERNEL_COMMAND="<build>/bin/duckc repl --framed" python3 -m unittest discover -s tests -t .
```

The tests only need the standard library; the second run drives a real REPL.

## Docs links
[Duckling docs](https://docs.duckling.pl/duckling/introduction/index.html) \
[Syntax highlighting plugin](https://pypi.org/project/jupyterlab-duckling-syntax-highlighting/)

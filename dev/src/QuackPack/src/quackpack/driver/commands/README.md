### quackpack.driver.commands

Implementations of CLI commands invoked by the user.

- Top-level commands:
  - `build.py`, `run.py`, `list.py`, `search.py`, `publish.py`, `remove.py`, `sync.py`, `unsync.py`, `cache.py`, `clean.py`, `info.py`, `tree.py`.
- Subpackages:
  - `add/`: logic for `qp add` (sources, types, handler).
  - `init/`: project initialization helpers (`execute.py`, file creators, types).

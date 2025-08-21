### quackpack.util

This directory is a collection of various utilities.

- **Highlights**
  - `logger.py`: Rich-based logging setup with `QP_DEBUG` filtering.
  - `env.py`: cross-platform environment access (case-insensitive keys on Windows).
  - `context_managers.py`, `global_context.py`, `cpu.py`, `xdg_directories.py`, `venv_config.py`, `toml_config.py`.
  - `types/`: shared error, versioning and identifiers.
  - `yaml/`: strict YAML loading/validation helpers with rich error messages.
  - `lock/`: filelock implementation for POSIX/Windows and software fallback.
  - `duckling_compatibility.py` and `types/pkgid.py`: identifier and naming helpers.
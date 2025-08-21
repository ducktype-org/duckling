# Global Quack Pack Configuration

This page explains the structure of global Quack Pack's configuration file.

## Filesystem location

Quack Pack looks for configuration file in the following locations, in this order:

- `$QP_CONFIG/config.toml`, if `$QP_CONFIG` is set,
- `$XDG_CONFIG_HOME/qp/config.toml`, if `$XDG_CONFIG_HOME` is set,
- otherwise:
  - `$HOME/.config/qp/config.toml` on Linux, Unix/POSIX, and macOS,
  - `%LOCALAPPDATA%\qp\config.toml` on Windows.

Quack Pack cache directory lives in first found place, in following order:

- `$QP_CACHE`, if `$QP_CACHE` is set,
- `$XDG_CACHE_HOME/qp/`, if `$XDG_CACHE_HOME` is set,
- otherwise:
  - `$HOME/.cache/qp/` on Linux, Unix/POSIX, and macOS,
  - `%LOCALAPPDATA%\caches\qp\` on Windows.

## Configuration file structure

Global configuration follows TOML file format, for ease of manual edition and reading it.

```toml
[aliases]
al = "some-cool-command --with-bunch-of-options"

[cache]
download_dir = "~/.cache/duck/qp"
metadata_db_path = "~/.qp_sqlite3"
fetcher_lockfile = "~/.cache/qp/lock"

[storage]
dir = "~/.local/share/qp/storage"
temporary_lifetime = "100"

[packaging]
build_from_source = false

[build.targets]
i686.compiler_flags = ["-flto"]

[build.profiles]
debug.compiler_flags = ["-O0", "-ggdb3"]

[registry]
default = "https://www.quackpack.com"

[security.typos]
enabled = true
max_distance = 10
```

### aliases

List of aliases for Quack Pack. Each alias is in form:

```toml
alias_name = "string representing alias"
```

Aliases are expanded by copy-pasting. In particular, from the example configuration,
`qp al --foo` is equivalent to `qp some-cool-command --with-bunch-of-options --foo`.

### cache

- `download_dir`: type is `String`. In this directory Quack Pack will keep downloaded archives of packages' source codes. Defaults to `CACHE_DIRECTORY/downloads`.

- `metadata_db_path`: type is `String`. This file points to SQLite3 database, where Quack Pack keeps metadata of dependencies. Defaults to `CACHE_DIRECTORY/metadata_db.sqlite`

- `fetcher_lockfile`: type is `String`. This file is used as a global lockfile for Quack Pack fetcher. Defaults to `CACHE_DIRECTORY/fetcher.lock`.

### storage

- `storage`: type is `String`. It points to directory, where Quack Pack keeps unpacked sources of resolved dependencies.

- `temporary_lifetime`: type is `Number`. During `clean` operation Quack Pack will remove any **temporary** virtual environments older than `temporary_lifetime`.

### packaging

These options affect how internally Quack Pack stores packages.

- `build_from_source`: type is `Boolean`. If set to `true`, Quack Pack will download source of every dependency and locally compile it to bytecode. Useful for projects, where performance is the key. Defaults to `false`.

### build

- `targets`: dictionary with extra compiler flags for different targets. Each value should be a dictionary with a single key `compiler_flags`, which points to a `List[String]`.

- `profiles`: dictionary with extra compiler flags for different profiles. Each value should be a dictionary with a single key `compiler_flags`, which points to a `List[String]`.

### registry

- `url`: type is `String. Default URL of Ducknest registry.

### security

This section consists of some *controversial* quality of life features.
We provide *sane and sensible* defaults.
If You wish to use any of those features, You should be aware, that they allow RCE.

#### typo_tolerance

This section configures automatic execution of best subcommand match, instead of printing it.

- `enabled`: type is `Boolean`, defaults to `false`. If set to `true`, Quack Pack will execute its best match, instead of notifying the user.

- `max_distance`: type is `Number`, defaults to `1`. Changes maximum Levenshtein distance for command execution. If multiple commands' distances are smaller or equal than `max_distance`, then none of them is executed, and error is thrown.

# Global Quack Pack Configuration

This page explains the structure of global Quack Pack's configuration file.

## Filesystem location

Quack Pack looks for configuration file in the following locations, in this order:

- `$QP_CONFIG/config.yaml`, if `$QP_CONFIG` is set,
- `$XDG_CONFIG_HOME/qp/config.yaml`, if `$XDG_CONFIG_HOME` is set,
- otherwise:
  - `$HOME/.config/qp/config.yaml` on Linux, and Unix/POSIX,
  - `$HOME/Library/Application Support/qp/config.yaml` on macOS,
  - `%LOCALAPPDATA%\qp\config.yaml` on Windows.

Quack Pack cache directory lives in first found place, in following order:

- `$QP_CACHE`, if `$QP_CACHE` is set,
- `$XDG_CACHE_HOME/qp/`, if `$XDG_CACHE_HOME` is set,
- otherwise:
  - `$HOME/.cache/qp/` on Linux, and Unix/POSIX,
  - `$HOME/Library/Caches/qp/` on macOS,
  - `%LOCALAPPDATA%\caches\qp\` on Windows.

## Configuration file structure

Global configuration follows YAML file format, for ease of manual edition and reading it.

```yaml
aliases:
  al: some-cool-command --with-bunch-of-options

cache:
  download_dir: $HOME/.cache/duck/qp
  metadata_db_path: $HOME/.qp_sqlite3
  fetcher_lockfile: $HOME/.cache/qp/lock
  max_size: 10G

storage:
  storage: $HOME/.local/share/qp/storage
  temporary_lifetime: 100

packaging:
  build_from_source: false

build:
  targets:
    i686:
      compiler_flags: [-flto]
  profiles:
    debug:
      compiler_flags: [-O0, -ggdb3 ]

repository:
  default: [ https://www.quackpack.com ]
  extra: [ https://another-cool-repo.com, ~/src/local-project ]

security:
  typo_tolerance:
    enabled: false
    max_distance: 1
```

### aliases

List of aliases for Quack Pack. Each alias is in form:

```yaml
alias_name: string representing alias
```

Aliases are expanded by copy-pasting. In particular, from the example configuration,
`qp al --foo` is equivalent to `qp some-cool-command --with-bunch-of-options --foo`.

#### NOTE:

You can either use `String` or `List[String]` as a type.
If it's not `List[String]`, an alias will be split by spaces, and this operation is not context aware!

That is, if You want to have:

```yaml
alias_name: build "Don't split me"
```

You should use:

```yaml
alias_name: [build, "\"Don't split me\""]
```

### cache

- `download_dir`: type is `String`. In this directory Quack Pack will keep downloaded archives of packages' source codes. Defaults to `CACHE_DIRECTORY/downloads`.

- `metadata_db_path`: type is `String`. This file points to SQLite3 database, where Quack Pack keeps metadata of dependencies. Defaults to `CACHE_DIRECTORY/metadata_db.sqlite`

- `fetcher_lockfile`: type is `String`. This file is used as a global lockfile for Quack Pack fetcher. Defaults to `CACHE_DIRECTORY/fetcher.lock`.

- `max_size`: maximum available size for cache. It's type is `<number><suffix>`, where `<suffix>` is one of `G`, `M`, or `K`, appropriately for gigabytes, megabytes and kilobytes.

### storage

- `storage`: type is `String`. It points to directory, where Quack Pack keeps unpacked sources of resolved dependencies.

- `temporary_lifetime`: type is `Number`. During `clean` operation Quack Pack will remove any **temporary** virtual environments older than `temporary_lifetime`.

### packaging

These options affect how internally Quack Pack stores packages.

- `build_from_source`: type is `Boolean`. If set to `true`, Quack Pack will download source of every dependency and locally compile it to bytecode. Useful for projects, where performance is the key. Defaults to `false`.

### dependency_solver

This subsection is responsible for configuring options of dependency solver.

- `Todo`: type is `True`. TODO: co tu dodać.

### build

- `targets`: dictionary with extra compiler flags for different targets. Each value should be a dictionary with a single key `compiler_flags`, which points to a `List[String]`.

- `profiles`: dictionary with extra compiler flags for different profiles. Each value should be a dictionary with a single key `compiler_flags`, which points to a `List[String]`.

### repository

- `default`: type is `String` or `List[String]`. Default URL or list of default URLs for Ducknest's servers.

- `extra`: type is `String` or `List[String]`. Provides extra repositories, from which packages can be downloaded. Defaults to `""`, which stands for no extra repositories. Each entry should implement sparse.

### security

This section consists of some *controversial* quality of life features.
We provide *sane and sensible* defaults.
If You wish to use any of those features, You should be aware, that they allow RCE.

#### typo_tolerance

This section configures automatic execution of best subcommand match, instead of printing it.

- `enabled`: type is `Boolean`, defaults to `false`. If set to `true`, Quack Pack will execute its best match, instead of notifying the user.

- `max_distance`: type is `Number`, defaults to `1`. Changes maximum Levenshtein distance for command execution. If multiple commands' distances are smaller or equal than `max_distance`, then none of them is executed, and error is thrown.

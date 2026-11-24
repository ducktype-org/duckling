# Documentation for future Duck and QuackPack developers

This file is the main entry point for a future duck and quackpack developer.

More detailed descriptions of various parts of duck and/or quackpack can be found in specific files dedicated to them.

## Required external dependencies

- `libgfortran5`,
- `libgit2`,
- `libssh2`,
- `libssl2`,
- `libsqlite3`,
- `pkg-config` on Linux, for finding OpenSSL installation.

Most of them could be avoided by passing appropriate `bundled-*` feature flag, or `bundled-all`, to bundle everything.

## General code structure

While duck is a binary and __the__ entry point for building Duckling programmes, quackpack is the engine behind package management.

The code is a Cargo workspace of three crates (this directory is its root):
```
.
├── src/        <`duck`: the CLI driver and the `duck` binary>
├── quackpack/  <`quackpack`: the package manager>
└── duck_core/  <`duck_core`: what every duck tool shares>
```

## More docs

- [from CLI to library: binary side of duck](src/readme.md)
- [editing the manifest file structure (schema)](quackpack/src/schemas/readme.md)
- [adding a new subcommand](src/driver/subcommands/readme.md)
- [storage overview](quackpack/src/core/storage/readme.md)
- [solving a dependency graph](quackpack/src/core/solver/solving/readme.md)
- [networking module](quackpack/src/core/fetcher/readme.md)

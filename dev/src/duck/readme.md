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
├── Cargo.toml   <workspace root, and the `duck` package>
├── src          <`duck`: the CLI driver and the `duck` binary>
│   ├── bin/duck/main.rs
│   └── lib/duck/<duck files>
├── quackpack    <`quackpack`: the package manager>
│   └── src/<quackpack files>
└── duck_utils   <`duck_utils`: what every duck tool shares>
    └── src/<shared files>
```
Dependencies go one way only: `duck` → `quackpack` → `duck_utils`.
`duck` calls into quackpack from its subcommands, and quackpack needs `DuckContext`, `QuackError` and the other shared types.
Keeping those shared types in `duck_utils` is what breaks the dependency cycle between duck and quackpack.

`duck_utils` holds what gives every tool the same UX and behaviour: `QuackError` and `QuackResult`, `DuckContext` (terminal, user configuration, duck home), shared `clap` helpers and styles, and locating a package root (`package_root`).
It does __not__ parse manifests: that is quackpack's job.
So an ecosystem tool (e.g. a formatter) depends on `duck_utils` only, and does not link in the solver, the fetcher or git.

Some functions only exist to mock the internal state in unit tests (e.g. `DuckContext::default`).
They live behind `#[cfg(any(test, feature = "test-utils"))]` in `duck_utils`: `#[cfg(test)]` alone is not visible to other crates, so `quackpack` and `duck` enable the `test-utils` feature in their `[dev-dependencies]`.
The binary stays a thin wrapper around `duck::main`, so all of duck can be unit tested as a library.

`cargo build`, `cargo test` and `cargo clippy` run from this directory cover the whole workspace (`default-members`).

## More docs

- [from CLI to library: binary side of duck](src/lib/duck/readme.md)
- [editing the manifest file structure (schema)](quackpack/src/schemas/readme.md)
- [adding a new subcommand](src/lib/duck/driver/subcommands/readme.md)
- [storage overview](quackpack/src/core/storage/readme.md)
- [solving a dependency graph](quackpack/src/core/solver/solving/readme.md)
- [networking module](quackpack/src/core/fetcher/readme.md)

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

That being said, both [share a single code directory](../), with the following structure:
```
src
├── bin
│   └── duck
│       └── main.rs
└── lib
    ├── duck
    │   └── <duck files>
    ├── quackpack
    │   └── <quackpack files>
    └── <more files>
```
The reasons behind this are not very obvious.
First of all, duck in its subcommands needs to call into quackpack, so naturally duck depends on quackpack.
However, quackpack uses types which are defined in duck.
As a consequence, this creates a local dependency cycle, which isn't supported by rust.

This could be solved in two ways: first is to factor out common types into a third local dependency.
Second is shown above: we have merged quackpack as a rust's submodule into duck package, but we have kept human-readable separation between the directories.

The second reason, not obvious at a first glance, is more subtle and related to unit tests.
It turns out that binary and library parts are internally treated as different packages, and when testing are built separately.
Problems arose when we tried to run unit tests.

We use `#[cfg(test)]` on some functions in order to mock the internal state in tests.
Since the library and binary are different packages, they are built separately for tests, so the `#[cfg(test)]` from the library is not available in the binary's `#[cfg(test)]`, and vice versa.
To solve this issue, we have moved __the entirety__ of duck into a library, and the binary is just a thin wrapper around a `duck::main` method, which in combination with the already merged quackpack, has allowed us to write unit test with an ease.

## More docs

- [from CLI to library: binary side of duck](src/lib/duck/readme.md)
- [editing the manifest file structure (schema)](src/lib/quackpack/schemas/readme.md)
- [adding a new subcommand](src/lib/duck/driver/subcommands/readme.md)
- [storage overview](src/lib/quackpack/core/storage/readme.md)
- [solving a dependency graph](src/lib/quackpack/core/solver/solving/readme.md)
- [networking module](src/lib/quackpack/core/fetcher/readme.md)

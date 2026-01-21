# Documentation for future Duck and QuackPack developers

This file is the main entry point for a future duck and quackpack developer.

More detailed descriptions of various parts of duck and/or quackpack can be found in specific files dedicated to them.

## Required external dependencies

- `libgit2`,
- `libssh2`,
- `libssl2`,
- `libsqlite3`,
- `pkg-config` on Linux, for finding OpenSSL installation.

Most of them could be avoided by passing appropriate `bundled-*` feature flag, or `all-bundled`, to bundle everything.

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


## But what happens under the hood of duck?

On the very high level, duck code execution can be broken into three steps: performing some general setup, parsing arguments, and taking an appropriate action.

### General setup

The very first function which is called is [`src/lib/duck/main.rs`](src/lib/duck/main.rs).
It's responsible only for four things: setting up debug loggers, creating the global [`DuckCtx`](src/lib/duck/util/duck_ctx.rs), calling [`src/lib/duck/driver/run.rs`](src/lib/duck/driver/run.rs), and catching any returned `Err`.

### Parsing

All of the parsing is done in [`src/lib/duck/driver/run.rs`](src/lib/duck/driver/run.rs).
Before actually passing command-line arguments to the parser, we:
- [try to fix an possible user typos in subcommand names,](src/lib/duck/driver/cli_args_preprocessing/typos_fixing.rs)
- [expand user aliases.](src/lib/duck/driver/cli_args_preprocessing/aliases_expansion.rs)

### Subcommand execution

Lastly we almost immediately call into the [`src/lib/duck/driver/subcommands/`](src/lib/duck/driver/subcommands/) directory, where the real execution begins.

## More docs

- [editing the manifest file structure (schema)](src/lib/quackpack/schemas/readme.md)
- [adding a new subcommand](src/lib/duck/driver/subcommands/readme.md)

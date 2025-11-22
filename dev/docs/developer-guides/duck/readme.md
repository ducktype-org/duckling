# Documentation for future Duck and QuackPack developers

This file is the main entry point for a future duck and quackpack development.

More detailed descriptions on various parts of duck and/or quackpack can be found in specific files dedicated to them.

## General code structure

While duck is a binary and __the__ entry point for building Duckling programmes, quackpack is an engine behind a package management.

That being said, both [share a single code directory](../../../src/duck/), with the following structure:
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
Reasons behind it are not very obvious.
First of all, duck in its subcommands needs to call into quackpack, so naturally duck depends on quackpack.
However, quackpack uses duck types.
As a consequence, this creates a local dependency cycle, which isn’t supported by rust.

This could be solved in two ways: first is to factor out common types into a third local dependency.
Second is shown above: we have merged quackpack as a rust’s submodule into duck package, but we have kept human-readable separation between the directories.

Second reason, not obvious at a first glance, is more subtle and related to unit tests.
It turns out that binary and library parts are internally treated as different packages, and when testing are build separately.
Problem have arisen, when we have tried to run unit tests.

We use `#[cfg(test)]` on some functions in order to mock internal state in tests.
Since library and binary are different packages, they are built separately for tests, so `#[cfg(test)]` from library is not available in binary's `#[cfg(test)]`, and vice versa.
To solve this issue, we have moved __whole__ duck into a library, and binary is just a thin wrapper around a `duck::main` method, which in combination with already merged quackpack, has allowed us to write unit test with an ease.
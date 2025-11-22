# Adding a new subcommand to duck

This document provides an example of adding a new subcommand, `foo`, to duck.

## General informations

Each subcommand consists of two parts: a parser, responsible for parsing command-line arguments, and an actual execution logic behind it.

## Adding new parser

All of the parser related code lives in [`src/lib/duck/driver/`](../../../src/duck/src/lib/duck/driver/) directory, and subcommands are in [`src/lib/duck/driver/subcommands/`](../../../src/duck/src/lib/duck/driver/subcommands/).
By convention, file for `foo` subcommand should be named `foo.rs`.

Parser should be provided by `get_parser` function, which type is `fn() -> Command`.
We also provide [`CommandExt`](../../../src/duck/src/lib/duck/driver/cli_ext.rs) trait with convenient methods to use when writing new subcommands.

After You have created `get_parser` function, it’s time to add it to the main parser.
All you need to do is to place it in the vector in the `subcommands` function in [`src/lib/duck/driver/subcommands/mod.rs`](../../../src/duck/src/lib/duck/driver/subcommands/mod.rs).
Be aware, that this list is order-aware, meaning that its order is reflected in the `--help` message!

Now the main parser should be aware of the `foo` subcommand, but we still need to add some logic behind it in order to execute some code.

## Subcommand code execution

Your `foo.rs` file should export one more function — `execute` (`fn(&DuckCtx, &ArgMatches) -> QuackResult<()>`), which actually executes some code.

First things first, You need to add Your `execute` function to the `match` statement in [`src/lib/duck/driver/subcommands/mod.rs`](../../../src/duck/src/lib/duck/driver/subcommands/mod.rs).
It should look like this:

```rust
// <other cases>
    "foo" => foo::execute, // "foo" is a name of the subcommand, without any aliases
// <more cases>
```

Notice that there are no brackets, since we don’t want to execute this function, but return a function pointer instead.

Also please note that [`src/lib/duck/driver/subcommands/`](../../../src/duck/src/lib/duck/driver/subcommands/) directory is not responsible for any complex actions.
Your `execute` should only collect arguments from command-line into some `struct`, and then call into [`src/lib/quackpack/subcommands/`](../../../src/duck/src/lib/quackpack/subcommands/) directory.
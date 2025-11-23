# But what happens under the hood of duck?

On the very high level, duck code execution can be broken into three steps: performing some general setup, parsing arguments, and taking an appropriate action.

## General setup

The very first function which is called is [`src/lib/duck/main.rs`](../../../src/duck/src/lib/duck/main.rs).
It's responsible only for four things: setting up debug loggers, creating the global [`DuckCtx`](../../../src/duck/src/lib/duck/util/duck_ctx.rs), calling [`src/lib/duck/driver/run.rs`](../../../src/duck/src/lib/duck/driver/run.rs), and catching any returned `Err`.

## Parsing

All of the parsing is done in [`src/lib/duck/driver/run.rs`](../../../src/duck/src/lib/duck/driver/run.rs).
Before actually passing command-line arguments to the parser, we:
- [try to fix an possible user typos in subcommand names,](../../../src/duck/src/lib/duck/driver/cli_args_preprocessing/typos_fixing.rs)
- [expand user aliases.](../../../src/duck/src/lib/duck/driver/cli_args_preprocessing/aliases_expansion.rs)

## Subcommand execution

Lastly we almost immediately call into the [`src/lib/duck/driver/subcommands/`](../../../src/duck/src/lib/duck/driver/subcommands/) directory, where the real execution begins.

More details can be found in the [`adding_new_subcommand.md`](adding_new_subcommand.md) document, or by reading the code.

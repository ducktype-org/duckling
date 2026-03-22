# But what happens under the hood of duck?

On the very high level, duck code execution can be broken into three steps: performing some general setup, parsing arguments, and taking an appropriate action.

## General setup

The very first function which is called is [`main.rs`](main.rs).
It's responsible only for four things: setting up debug loggers, creating the global [`DuckCtx`](util/duck_ctx.rs), calling [`driver/run.rs`](driver/run.rs), and catching any returned `Err`.

## Parsing

All of the parsing is done in [`driver/run.rs`](driver/run.rs).
Before actually passing command-line arguments to the parser, we:
- [try to fix possible user typos in subcommand names](driver/cli_args_preprocessing/typos_fixing.rs),
- [expand user aliases](driver/cli_args_preprocessing/aliases_expansion.rs).

## Subcommand execution

Lastly we almost immediately call into the [`driver/subcommands/`](driver/subcommands/) directory, where the real execution begins.

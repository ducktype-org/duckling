# Adding new subcommand to Quack Pack

This document provides an example of adding new subcommand, `foo`, to the Quack Pack.

## General informations

Each subcommands consists of two parts: parser, responsible for parsing command-line arguments, and actual execution login behind it.

## Adding new parser

All parser related code lives in [`src/quackpack/cli/subcommands/`](../../src/quackpack/cli/subcommands/) directory.
By convention, file for `foo` subcommand should be named `_foo_parser.py`.

Parser should be provided by `get_parser` function, which type is `Function() -> Parser`.
`Parser` is our internal wrapper around `argparse`, and it lives in [`src/quackpack/cli/subcommands/_parser.py`](../../src/quackpack/cli/subcommands/_parser.py).
It allows us to create parsers in OOP manner, without necessity of local variables, and should make any future porting a lot easier,
since we'd need to update only one file.

If You feel like [`src/quackpack/cli/subcommands/_parser.py`](../../src/quackpack/cli/subcommands/_parser.py) doesn't provide some functionality, feel free to add it!

After You created `get_parser` function, it's time to add it to the main parser.
All you need to do is to place it in `subcommands` list in `subcommands` function in [`src/quackpack/cli/subcommands/__init__.py`](../../src/quackpack/cli/subcommands/__init__.py).
Be aware, that this list is order-aware, meaning that it's order is reflected in the `--help` message!

Now main parser should be aware of the `foo` subcommand, but we still need to add some logic behind it to execute some code.

## Subcommand code execution

Your `_foo_parser.py` file should export one more function — `execute` (`Function(GlobalContext, Arguments)`), which actually executes some code.

First things first, You need to add Your `execute` function to the `match` statement in [`src/quackpack/cli/subcommands/__init__.py:action_for`](../../src/quackpack/cli/subcommands/__init__.py).
It should look like this:

```python
...
case "foo":
    return _foo_parser.execute
...
```

Notice, that there are no brackets, since we don't want to execute this function, but return function pointer/object.

Also please note, that [`cli/subcommands/`](../../src/quackpack/cli/subcommands/) directory is not responsible for any complex action.
Your `execute` should only collect arguments from command-line into some `dataclass`, and then call into [`src/quackpack/commands/`](../../src/quackpack/commands/) directory.

### On `commands/` directory

For Your subcommand You can use either plain files, and in that case name them `foo.py`, or in case of more complex logic,
a directory, and it should be named `foo/`, just like Your subcommand.

Here lies the implementation of parsing and using manifest in a machine-friendly format.

Main objects here are: `Summary` and `Manifest` types and `parse::parse_manifest()` function.

## Manifest parsing

Parsing manifest is done in three steps:
1. read file contents,
1. parse it into schema (here and only here pydantic should be used),
1. parse schema into `Manifest`.

Main job of `parse.py` is the last point, especially parsing dependencies sources.

Look in [here](../../../docs/dev/editing_manifest_schema.md) for some deeper explanation.

# Editing manifest schema

There are several things you need to do, when introducing new field into the manifest:
- update [`src/quackpack/core/types/manifest/schemas/manifest.py`](../../src/quackpack/core/types/manifest/schemas/manifest.py) schema, so the new field can be parsed by pydantic,
- add new class/member in `manifest`: please don't use `None`s or `str`s, one of the tasks of parsing is to also put sensible defaults and cast types,
- update [`src/quackpack/core/types/manifest/parse.py`](../../src/quackpack/core/types/manifest/parse.py), so your new field can be parsed.

# Editing registry schema

Sometimes manifest schema changes should be reflected in the registry schema too.
To edit registry schema you need to:
- update [`src/quackpack/core/types/manifest/schemas/registry.py`](../../src/quackpack/core/types/manifest/schemas/registry.py) file. For registry API don't use `None`s or opaque fields like in the manifest schema:
    humans don't need to read this file (and probably never will), it needs to be as simple as it can be
    (f.e. in `manifest` schema source is very complex type with not so obvious exclusive fields; for `registry` we are using tagged unions/enums).
- add appropriate `into_schema/from_schema` methods in your class; pyright should also guide you to any callers, which should be updated,
- **update schema on the Ducknest side**.

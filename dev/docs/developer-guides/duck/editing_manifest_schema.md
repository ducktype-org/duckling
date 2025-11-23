# Editing the manifest schema

There are several things you need to do when introducing a new field to the manifest:
- update the [`src/lib/quackpack/schemas/manifest.rs`](../../../src/duck/src/lib/quackpack/schemas/manifest.rs) schema, so the new field can be parsed by serde,
- add a new member to the  [`core::Manifest`](../../../src/duck/src/lib/quackpack/core/manifest/manifest_struct.rs) machine friendly struct; please don't use `Option`s or `StrId`s, one of the tasks of the parsing is to put sensible defaults and cast serde's types into more friendly ones,
- update the appropriate file in the [`src/lib/quackpack/core/parse/`](../../../src/duck/src/lib/quackpack/core/parse) directory, so your new field can be parsed.

# Editing the registry schema

Sometimes, a manifest schema change should be reflected in the registry schema too.
To edit the registry schema you have to:
- update the [`src/lib/quackpack/schemas/registry.rs`](../../../src/duck/src/lib/quackpack/schemas/registry.rs) file. For registry API don't use `Option`s or opaque fields like in the manifest schema:

    humans don't need to read this file (and probably never will), it needs to be as simple as possible
    (e.g. in the `Manifest`'s schema source, there is a fairly complex type with not so obvious exclusive fields, whereas for the `Registry` we are using tagged enums).
- add appropriate `From` and `Into` trait impls to your struct

    rust-analyzer should guide you to any callers which have to be updated,
- __update schema on the Ducknest side__.
# Archiver

Thin wrapper around the system `ar` tool. Bundles a list of object file
artifacts into a single static library (`.a`) via `ar rcs`. Used by the
driver when a task selects `strategy: "lib"`.


`createArchive` runs:

```
<archiver> rcs <output.a> <input1.o> <input2.o> ...
```

Flags: `r` insert/replace, `c` silently create, `s` write the symbol
index. The output `.a` is **deleted first** if it already exists, so the
archive is always built fresh and never inherits stale members from a
previous run.

## Notes for callers

- Pass **only object files** in `inputs`. The archiver does not
  recursively unpack other `.a` files.
- `output` and every `inputs[i]` must be valid `artifacts::FileArtifact`
  instances — their on-disk paths are taken verbatim.
- `createArchive` is a leaf operation: it does not register any query
  side inputs.

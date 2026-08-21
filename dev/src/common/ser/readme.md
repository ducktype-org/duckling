# Ser module

Header-only binary serialization for C++23. Scope: **bytes <-> object**. The module has no
concept of a file, no dependencies, and does not use anything from `base/`.

Imported from the standalone `ser` repository and then adapted to this repository's
formatting and naming, so the two copies have diverged on purpose - see
[Relation to the upstream library](#relation-to-the-upstream-library).

# Interface

## Files

* `ser/ser.hpp` - the umbrella: archives, dispatch, the envelope. Include this.
* `ser/std/all.hpp` - adapters for `string`, `vector`, `optional`, `pair`, `tuple`,
  `map`, `set`, `unordered_map`, `unordered_set`. An **opt-in** include: without it
  `ser/ser.hpp` never learns those types exist and never pays for `<map>` to serialize
  something else.
* `ser/macros.hpp` - `SER_DESCRIBE`, `SER_DESCRIBE_MAKE`, `SER_MAKE_FROM`, `SER_FRIEND`.
* `ser/test.hpp` - `SER_TEST_ROUNDTRIP(sample)`, which writes, reads back and names the
  field that came back different.

## Symbols

Everything is in namespace `ser`.

~~~~~cpp
#include <ser/ser.hpp>
#include <ser/std/all.hpp>

struct Point {
	i32         x;
	i32         y;
	std::string name;
};  // no serialization code at all

std::vector<std::byte> buf;
ser::write(buf, Point{ 1, 2, "origin" }).orThrow();

auto p = ser::read<Point>(std::span<const std::byte>{ buf });
if (!p) { /* p.code() is a ser::Errc */ }
Point back = std::move(*p).take();
~~~~~

* `ser::write(buf, x, opts = {})` -> `ser::result<>`. **Appends** into a growable buffer,
  so `buf.size()` is always exactly the bytes produced so far.
* `ser::read<T>(bytes, opts = {})` -> `ser::result<ser::owned<T, Ctx>>`. Never a bare `T`:
  `owned` is what will carry the pools in a later version. `*r` is the bundle, `r->value`
  the object, `std::move(*r).take()` moves it out.
* `ser::readOrThrow<T>` for a type that cannot be moved (the return type equals the
  returned prvalue, so the object is built once, at its final address), and
  `ser::readOrThrowForce<T>` when the bare object is all you want.
* `ser::Errc` / `ser::error` / `ser::result<T>` - errors are codes, and the position of
  the byte that failed comes with them. Only the `*OrThrow` entry points throw
  (`ser::exception`).

## Serializing your own type

Four ways, in the order the library consults them:

1. A `ser::serializer<T>` specialization - the hook for a type you cannot edit. This is
   what a `STRONG_TYPEDEF_INT` needs, and `ser_test.cpp` has the one for `u8`.
2. In-class hooks: `serVisit`, or `serWrite` + `serRead`, or `serWrite` + `serMake`.
   They may be private behind `SER_FRIEND`.
3. The same three shapes found by ADL, for a type in someone else's namespace.
4. Nothing at all: an aggregate is walked field by field through structured bindings.

`serMake` is what makes a type with `const` fields, no default constructor or no move
constructor readable: its prvalue is returned into the caller's object directly.
`SER_DESCRIBE(a, b)` writes the `serVisit` for you, `SER_DESCRIBE_MAKE(T, a, b)` the
`serWrite` + `serMake` pair.

The automatic walk cannot count the fields of a type with a **C array field** (it counts
one clause per element, so the count comes out too high) or with **private** fields; say
`using ser_members = ser::members<N>;` there. Getting it wrong is a compile error inside
the bindings ladder, never wrong bytes. A **base class** and a **reference field** are
refused outright.

# Notes

* The envelope (`ser::options{ .header = true }`) is 32 bytes: magic, `schema_hash`,
  payload size, a CRC slot and platform flags. Off by default, so a plain `ser::write` is
  exactly the payload.
* `ser::schemaHash<T>()` is `consteval` and hashes the **wire**, not the layout, so two
  platforms that agree on the format agree on the number. Field names are deliberately
  absent from it; they go into `ser::debugHash<T>()`, which is diagnostics only. Equal
  `schema_hash` with different `debug_hash` is exactly "somebody renamed a field".
* Not here yet: pools (`Box`/`Ref`/`StrID`), `variant`, `chrono`, CRC32C, zero-copy views.

# Relation to the upstream library

The headers came from the standalone `ser` repository and were then put through
`clang-format` and `clang-tidy`'s `readability-identifier-naming` against this
repository's configuration. That renamed roughly 250 identifiers, including the hook
names a type dispatches on:

| upstream          | here          |
|-------------------|---------------|
| `ser_visit`       | `serVisit`    |
| `ser_write`       | `serWrite`    |
| `ser_read`        | `serRead`     |
| `ser_make`        | `serMake`     |
| `ser::errc::ok`   | `ser::Errc::Ok` |
| `ser::schema_hash`| `ser::schemaHash` |
| `min_wire_size_v` | `MIN_WIRE_SIZE_V` |

`ser_members` keeps its name: it is a type alias, and the convention leaves those alone.

The two copies are therefore no longer mergeable by `git`. Porting a change from upstream
means applying it by hand and running `clang-format` plus
`toolbox.py cpp-linter --all` over the module afterwards.

Four suppressions survive in the module, each one line above the code it covers and each
with a reason: the C-array partial specializations in `builtin/array.hpp` and
`detail/dispatch.hpp` (specializing on a C array is their subject), `magic[8]` in
`stream/header.hpp` (a fixed field of the wire layout), `printf` in `test.hpp` and
`debug.hpp` (neither may drag `<print>` into every translation unit), and the version
macros in `config.hpp` (they have to work in an `#if`).

# Regenerating the tables

`src/ser/detail/ladder.inc`, `ladder_decls.inc` and `for_each.inc` are generated and
committed. Regenerate rather than hand-edit, and re-format afterwards - the generators
emit upstream's layout, and the committed files are the formatted ones:

~~~~~bash
cd src/common/ser
python3 tools/gen_ladder.py     # ladder.inc and ladder_decls.inc
python3 tools/gen_for_each.py   # for_each.inc
for f in src/ser/detail/*.inc; do
	clang-format-19 -assume-filename=x.cpp "$f" > "$f.tmp" && mv "$f.tmp" "$f"
done
~~~~~

# Tests

`tests/ser_test.cpp` is six tests over two deeply nested structures - one for the data
shapes (four levels of aggregate, every std adapter, both enum kinds, a C array field, a
strong typedef), one for the dispatch ladder (all four hook levels plus the macros, each
stamping a distinct byte so the test proves *which* rung ran). It runs in about 0.13 s.

Upstream's own suite is 205 cases over ten files plus 33 compile-failure targets; those
are not ported, and this repository has no harness for compile-failure tests.

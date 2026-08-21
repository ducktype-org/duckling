# Ser module

Header-only binary serialization for C++23. Scope: **bytes <-> object**. The module has no
concept of a file and no dependencies beyond `base/`, from which it uses the preprocessor
helpers (`FOR_EACH`, `FOR_EACH_COMMA`, `CAT`, `STRINGIFY_2`) and nothing else.

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
* `ser/base/all.hpp` - adapters for the `base` types, opt-in the same way. `Optional`,
  `Map`, `HashMap`, `VectorMap`, `StableHashMap`, `StableVector`, `Box`, `MBox`,
  `OwningView`, `SharedView`, `DynamicBitset`, `Bit256` serialize; `Ref`/`CRef`/`MRef`,
  `SharedBox`, `BoxOrCRef`, `RawView`/`ModRawView` and `CheckedOkBad` are **refused** with
  a message naming what to write instead, which is only visible if the header is included.
  `OkBad` and `Monostate` need nothing - they are aggregates.
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

The module also uses `base/preproc` where upstream generates code with Python, and folds
the repeated hook detectors into the macros in `src/ser/detail/hooks.hpp`.

The two copies are therefore no longer mergeable by `git`. Porting a change from upstream
means applying it by hand and running `clang-format` plus
`toolbox.py cpp-linter --all` over the module afterwards.

Four suppressions survive in the module, each one line above the code it covers and each
with a reason: the C-array partial specializations in `builtin/array.hpp` and
`detail/dispatch.hpp` (specializing on a C array is their subject), `magic[8]` in
`stream/header.hpp` (a fixed field of the wire layout), `printf` in `test.hpp` and
`debug.hpp` (neither may drag `<print>` into every translation unit), and the version
macros in `config.hpp` (they have to work in an `#if`).

# The structured-bindings ladder

A structured binding declaration spells its arity out, so "hand me every member" needs one
branch per member count. `src/ser/detail/ladder.hpp` holds that table as macros:
`SER_DETAIL_LADDER(RUNG)` expands `RUNG(n)` for every count up to
`ser::detail::LADDER_MAX`, and `SER_DETAIL_LADDER_NAMES(n)` hands a rung its `n` binding
names. `ser::access` uses it twice - once to walk the members, once to report their
declared types - and `detail::MAX_MEMBERS` is taken from `LADDER_MAX`, so nothing can
drift apart. Upstream generates the same table with a Python script; here it is
plain preprocessor, and a typo in it is a duplicate or a missing binding, which is a
compile error either way.

# Tests

`tests/ser_test.cpp` is six tests over two deeply nested structures - one for the data
shapes (four levels of aggregate, every std adapter, both enum kinds, a C array field, a
strong typedef), one for the dispatch ladder (all four hook levels plus the macros, each
stamping a distinct byte so the test proves *which* rung ran). It runs in about 0.13 s.

`tests/ser_base_test.cpp` is ten tests over `ser/base/all.hpp`: what each base type does on
the wire, which streams are interchangeable with their std form, and what a damaged one
gets back. The refusals cannot be tested from here - a `static_assert` that fires is a
build failure, not a failing case - so what it pins instead is that the types next to a
refused one still work.

Upstream's own suite is 205 cases over ten files plus 33 compile-failure targets; those
are not ported, and this repository has no harness for compile-failure tests.

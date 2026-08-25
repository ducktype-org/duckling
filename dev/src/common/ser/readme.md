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
* `ser/std/all.hpp` - adapters for the std types, one header per standard header they
  wrap: whatever `src/ser/std/` contains is what is supported, and `variant` includes
  `std::monostate`. An **opt-in** include: without it `ser/ser.hpp` never learns those
  types exist and never pays for `<map>` to serialize something else.
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
ser::writeOrPanic(buf, Point{ 1, 2, "origin" });

auto p = ser::read<Point>(std::span<const std::byte>{ buf });
if (!p) { /* p.code() is a ser::Errc */ }
Point back = std::move(*p).take();
~~~~~

* `ser::write(buf, x, opts = {})` -> `ser::result<>`. **Appends** into a growable buffer,
  so `buf.size()` is always exactly the bytes produced so far.
* `ser::read<T>(bytes, opts = {})` -> `ser::result<ser::owned<T, Ctx>>`. Never a bare `T`:
  `owned` is what will carry the pools in a later version. `*r` is the bundle, `r->value`
  the object, `std::move(*r).take()` moves it out.
* `ser::readOrPanic<T>(bytes)` / `ser::writeOrPanic(buf, x)` - the same two, for bytes
  whose validity is an invariant of the program rather than input: a bad stream is
  `CORE_PANIC` instead of a code nobody was going to recover from. `readOrPanic` is also
  what reads a type that cannot be MOVED, because its return type equals the returned
  prvalue and the object is built once, at its final address; `ser::readOrPanicForce<T>`
  is the same thing returning a bare `T`.
* `ser::Errc` / `ser::error` / `ser::result<T>` - errors are codes, and the position of the
  byte that failed comes with them. **No entry point throws**: an exception is how the
  by-value read path reports inside the library (`ser::detail::readThrowing`), and every
  public entry point turns it into a `result` or a panic before it can escape. The panic is
  real in every build type - `CORE_PANIC` alone is `std::unreachable()` in Release, so the
  panicking forms do not rely on it and abort loudly there instead. It is still a stop, so
  the choice between the two is whether the caller has anything to do afterwards: bytes off
  a disk that is allowed to be damaged want `ser::read`.

## Serializing your own type

Four ways, in the order the library consults them:

1. A `ser::serializer<T>` specialization - the hook for a type you cannot edit, and the one
   that outranks everything below. `base::StrID` is one (in `string_id.hpp`), and every
   adapter under `base/` and `std/` is another.
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

A `STRONG_TYPEDEF_INT` and a `STRONG_TYPEDEF_INT_DIMENSIONAL` need none of this: the macro
declares a `serVisit` over the wrapped integer, so a strong typedef - and any struct with a
field of one - round-trips with no code at all. The same macro also declares
`ser_schema_as` and `ser_schema_name`, which is what keeps two typedefs over the same
integer apart in `schemaHash` - see [Telling a wrapper's format](#the-schema-hash) below.

# Who uses it

* **Query framework.** `QueryGraph::ReducedGraphData` is the graph's wire form and needs no
  code at all; `MetadataStorage` has the one hand-written `serWrite`/`serRead` pair in the
  repository, because its format is a type table plus whatever each metadata instance writes
  through `BaseMetadata::serWrite` - a virtual, which is why the archive is pinned to
  `MetadataOut`/`MetadataIn` there. `DECLARE_METADATA` needs nothing from the wrapped type.
* **Side-input keys** (`KeyOf_ModuleChildSideInput`, `KeyOf_PackageDependencyAliasSideInput`)
  are plain aggregates that travel as metadata.
* **`artifacts`** blobs: `setData<T>` / `getData<T>` are `ser::write` / `ser::read` with the
  `RawView` plumbing around them.
* **`debug_info`**: aggregates all the way down. The one shape that needs an adapter rather
  than the field walk is the `std::variant` in `SourcePosition`, and `<ser/std/variant.hpp>`
  is that adapter, so the structure itself carries no serialization code.

Two conventions came out of those: a stream that fails to READ is information, so the caller
logs it and carries on without the cache, while a failure to WRITE what we are already
holding is `CORE_PANIC`. Both are now spelled in the library rather than at each call site -
`ser::read` for the first, `ser::writeOrPanic` / `ser::readOrPanic` for the second - and no
`ser::exception` can escape into the compiler because no public entry point throws one.

# Notes

* The envelope (`ser::options{ .header = true }`) is 32 bytes: magic, `schema_hash`,
  payload size, a CRC slot and platform flags. Off by default, so a plain `ser::write` is
  exactly the payload.
* The envelope is **off at every call site in this repository**, so nothing above is
  schema-checked today: the graph, the metadata, a blob and a `.di` file are all bare
  payloads. What stands in for it is per-format (the metadata type table, the graph's
  `isConsistent()`), and a blob has nothing at all - turning the envelope on there is the
  open decision, not a missing feature.

<a name="the-schema-hash"></a>
* `ser::schemaHash<T>()` is `consteval` and hashes the **wire** field by field, never the
  layout: padding and alignment stay out, so a change in either does not invalidate a
  stream that is still readable. It is deliberately **not a portable number** - the root
  mixes `nativeFlags()`, the byte order and the pointer and length widths - because its job
  is to refuse a stream this program cannot read back, not to prove two platforms describe
  the same format. So pin the *relations* in tests (which types agree, what has to change
  the number) and pin a literal only per toolchain.
* A type whose format `ser` cannot see - a hand-written `serWrite`, a class with a private
  member - hashes as `"hook"` plus `sizeof` and `alignof`, which two unrelated types can
  share. Three ways to say what it really writes, in the order `schemaHash` consults them:
  a `ser::schema<T>` specialization (every `std`/`base` adapter has one, and so does
  `base::StrID`), an in-class `using ser_schema_as = W;` with an optional
  `ser_schema_name` for a type that cannot reach into namespace `ser` (every
  `STRONG_TYPEDEF_INT` uses this), or `ser::config<T>::schema_id`.
* Field names are deliberately absent from `schemaHash`; they go into `ser::debugHash<T>()`,
  which is diagnostics only. Equal `schema_hash` with different `debug_hash` is exactly
  "somebody renamed a field" - for a type that lists its fields with `SER_DESCRIBE`, which
  is the only place the names exist at all.
* Not here yet: pools (`Box`/`Ref`/`StrID`), `chrono`, CRC32C, zero-copy views.

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

`tests/ser_test.cpp` is eight tests over a handful of structures - one for the data shapes
(four levels of aggregate, every std adapter, both enum kinds, a C array field, a strong
typedef), one for the dispatch ladder (all four hook levels plus the macros, each stamping a
distinct byte so the test proves *which* rung ran), one for the variant, one for the
envelope, and two for `schemaHash`: `formatContract` pins which types agree and
`recursiveTypes` pins a type that contains itself - which has no hash at all unless the
schema walk turns meeting itself into a back-reference. It runs in about 0.11 s.

`tests/ser_base_test.cpp` is ten tests over `ser/base/all.hpp`: what each base type does on
the wire, which streams are interchangeable with their std form, and what a damaged one
gets back. The refusals cannot be tested from here - a `static_assert` that fires is a
build failure, not a failing case - so what it pins instead is that the types next to a
refused one still work.

Upstream's own suite is 205 cases over ten files plus 33 compile-failure targets; those
are not ported, and this repository has no harness for compile-failure tests.

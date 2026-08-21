# Ser module

Header-only binary serialization for C++23. Scope: **bytes <-> object**. The module has no
concept of a file, no dependencies, and does not use anything from `base/`.

Imported from the standalone `ser` repository. The headers under `src/ser/` are that
library verbatim - keep them that way, so the two stay mergeable. Everything duckling-side
lives in this file, `CMakeLists.txt`, `src/ser/ser.cpp` and `tests/`.

# Interface

## Files

* `ser/ser.hpp` - the umbrella: archives, dispatch, the envelope. Include this.
* `ser/std/all.hpp` - adapters for `string`, `vector`, `optional`, `pair`, `tuple`,
  `map`, `set`, `unordered_map`, `unordered_set`. An **opt-in** include: without it
  `ser/ser.hpp` never learns those types exist.
* `ser/macros.hpp` - `SER_DESCRIBE`, `SER_DESCRIBE_MAKE`, `SER_MAKE_FROM`, `SER_FRIEND`.
* `ser/test.hpp` - `SER_TEST_ROUNDTRIP(sample)`, which writes, reads back and names the
  field that came back different.

## Symbols

Everything is in namespace `ser`.

~~~~~cpp
#include <ser/ser.hpp>
#include <ser/std/all.hpp>

struct Point { i32 x, y; std::string name; };   // no code needed at all

std::vector<std::byte> buf;
ser::write(buf, Point{ 1, 2, "origin" }).or_throw();

auto p = ser::read<Point>(std::span<const std::byte>{ buf });
if (!p) { /* p.code() is a ser::errc */ }
Point back = std::move(*p).take();
~~~~~

* `ser::write(buf, x, opts = {})` -> `ser::result<>`. **Appends** into a growable buffer,
  so `buf.size()` is always exactly the bytes produced so far.
* `ser::read<T>(bytes, opts = {})` -> `ser::result<ser::owned<T, Ctx>>`. Never a bare `T`:
  `owned` is what will carry the pools in a later version. `*r` is the bundle, `r->value`
  the object, `std::move(*r).take()` moves it out.
* `ser::read_or_throw<T>` for a type that cannot be moved (the return type equals the
  returned prvalue, so the object is built once, at its final address), and
  `ser::read_or_throw_force<T>` when the bare object is all you want.
* `ser::errc` / `ser::error` / `ser::result<T>` - errors are codes, and the position of
  the byte that failed comes with them. Only the `*_or_throw` entry points throw
  (`ser::exception`).

## Serializing your own type

Four ways, in the order the library consults them:

1. A `ser::serializer<T>` specialization - the hook for a type you cannot edit.
2. In-class hooks: `ser_visit`, or `ser_write` + `ser_read`, or `ser_write` + `ser_make`.
   They may be private behind `SER_FRIEND`.
3. ADL hooks of the same three shapes, for a type in someone else's namespace.
4. Nothing at all: an aggregate is walked field by field through structured bindings.

`ser_make` is what makes a type with `const` fields, no default constructor or no move
constructor readable: its prvalue is returned into the caller's object directly.
`SER_DESCRIBE(a, b)` writes the `ser_visit` for you, `SER_DESCRIBE_MAKE(T, a, b)` the
`ser_write` + `ser_make` pair.

The automatic walk cannot count the fields of a type with a **C array field** (it counts
one clause per element) or with **private** fields; say `using ser_members =
ser::members<N>;` there. A **base class** and a **reference field** are refused outright.

# Notes

* The envelope (`ser::options{ .header = true }`) is 32 bytes: magic, `schema_hash`,
  payload size, a CRC slot and platform flags. Off by default, so a plain `ser::write` is
  exactly the payload.
* `ser::schema_hash<T>()` is `consteval` and hashes the **wire**, not the layout, so two
  platforms that agree on the format agree on the number. Field names are deliberately
  absent from it; they go into `ser::debug_hash<T>()`, which is diagnostics only.
* Not here yet: pools (`Box`/`Ref`/`StrID`), `variant`, `chrono`, CRC32C, zero-copy views.

# Regenerating the tables

`src/ser/detail/ladder.inc`, `ladder_decls.inc` and `for_each.inc` are generated and
committed. Regenerate rather than hand-edit:

~~~~~bash
cd src/common/ser
python3 tools/gen_ladder.py     # ladder.inc and ladder_decls.inc
python3 tools/gen_for_each.py   # for_each.inc
~~~~~

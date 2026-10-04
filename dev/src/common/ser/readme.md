ser
===

A binary serialization library for C++23, header-only, in namespace `ser`. Objects go to
bytes and come back. There are no files, no sockets, no
compression and no dependency beyond `base/`.

Contents
--------
* [Motivation](#motivation)
* [Introduction](#introduction)
* [Error Handling](#error-handling)
* [Error Codes](#error-codes)
* [What Works With No Code At All](#what-works-with-no-code-at-all)
* [Serializing Your Own Type](#serializing-your-own-type)
* [The Macros](#the-macros)
* [When The Automatic Walk Needs Help](#when-the-automatic-walk-needs-help)
* [What ser Refuses](#what-ser-refuses)
* [Archives: Several Messages In One Buffer](#archives-several-messages-in-one-buffer)
* [The Envelope](#the-envelope)
* [Schema Hash](#schema-hash)
* [Testing Your Format](#testing-your-format)
* [The Wire Format At A Glance](#the-wire-format-at-a-glance)
* [Limits And Knobs](#limits-and-knobs)
* [C++23 Today, C++26 Later](#c23-today-c26-later)
* [Compilers](#compilers)
* [Limitations](#limitations)

Motivation
----------
* Round-trip an object to binary with as little code as possible - for most types, none.
* Refuse at compile time, with a message that names the fix, anything whose meaning would
  not survive the trip. A pointer is a compile error, never a wrong byte.
* Treat a stream as untrusted input: every length is checked before anything is allocated,
  every `bool` comes back validated, and recursion has a ceiling.

Introduction
------------
For most types, enabling serialization takes no additional lines of code. It is enough
that the type is an aggregate with public fields - the fields are found through structured
bindings and written in declaration order.

```cpp
struct Person {
    std::string name;
    int         age{};
};
```

Writing it into a vector of bytes and reading it back:

```cpp
#include <ser/ser.hpp>
#include <ser/std/all.hpp>   // opt-in: teaches ser about std::string and friends

std::vector<std::byte> buf;
ser::writeOrPanic(buf, Person{ "Person1", 25 });

auto r = ser::read<Person>(buf);          // -> ser::result<ser::owned<Person>>
if (!r) { /* r.code() says why */ }
Person back = std::move(*r).take();
```

Three things about that snippet are worth knowing right away.

`ser::write` **appends**. Into a growable buffer each call starts where the last one
stopped, so `buf.size()` is always exactly the bytes produced so far and two writes give
you one stream. Into a fixed buffer (`std::span<std::byte>`) there is nothing to append to,
so each call fills it from the front and a buffer that is too small gives you
`Errc::BufferFull` rather than a resize.

`ser::read` never hands back a bare `T`. It returns `ser::owned<T>`, a bundle that will
carry object pools in a later version; `*r` is the bundle, `r->field` reaches through it,
and `std::move(*r).take()` moves the object out. `ser::readOrPanicForce<T>` is the one form
that returns the plain object.

`<ser/ser.hpp>` is the only header you have to remember. The entry points, the hook
vocabulary, every `SER_*` macro and `SER_TEST_ROUNDTRIP` all arrive with it, and nothing
under `ser/internal/` is yours to include.

The two adapter sets are the exception, and the `std/all.hpp` include above is deliberate.
They are separate so that `<ser/ser.hpp>` never pays for `<map>` to serialize something that
has nothing to do with maps. Include them before the first write or read of a type that uses
them - the ordinary rule for any trait specialization. So the complete list of headers a
caller ever needs is three: `<ser/ser.hpp>`, `<ser/std/all.hpp>`, `<ser/base/all.hpp>`.

Error Handling
--------------
Where the bytes came from decides which entry point you want, and the choice is not a
matter of taste.

Bytes that are **input** - a file somebody else wrote, a cache from an older build,
anything allowed to be wrong - go through `ser::read`, which hands back a code:

```cpp
if (const auto r = ser::read<Config>(bytes); !r) {
    log("cache unusable: {}", r.err().message());   // "stream Truncated (at position 34)"
    return {};                                      // carry on without it
}
```

Bytes that are **ours** - a blob this program wrote a moment ago, an object we are already
holding on the way out - use the panicking forms. A failure there is not information, it is
a broken program, and there is no state to recover to:

```cpp
ser::writeOrPanic(buf, node);                          // writing what we hold cannot fail
auto  owned = ser::readOrPanic<Node>(blob);            // returns ser::owned<Node>
Node  n     = ser::readOrPanicForce<Node>(blob);       // returns a bare Node
```

Reading into an object you already hold - `ser::in ar{bytes}; ar(obj);` - overwrites it. For
`base::StableVector` that object has to be **empty**: the container promises a `Ref` handed
out stays valid, so the adapter refuses a non-empty target rather than clearing it and
dangling every reference. `std::vector` has no such promise and is simply cleared.

`readOrPanic` is also what reads a type that **cannot be moved**: its return type is the
type of the returned prvalue, so the object is built once, at its final address.

Do not reach for a panicking form to keep a call site tidy. `CORE_PANIC` is
`std::unreachable()` in a Release build, so a panicking form handed bytes that are allowed
to be wrong is undefined behaviour there, with no diagnostic. Input goes through
`ser::read`. Always.

No public entry point throws. If
you prefer an exception at your own call site, `ser::result` has `.orThrow()`, which raises
`ser::exception` carrying the same code and position.

`ser::read` is handed a buffer whose size it knows, so it also requires the object to
account for **all** of it: bytes left over are `TrailingBytes`, which is what catches a
record read as a shorter one, or as a differently shaped one that happens to fit. Several
messages appended into one buffer are read with an archive rather than with `ser::read` -
see [Archives: Several Messages In One Buffer](#archives-several-messages-in-one-buffer) -
and nothing there changes.

Error Codes
-----------
`ser::Errc`, and every error carries the stream position that failed
(`r.err().position`, or the whole thing as text through `r.err().message()`).

| code | means |
|---|---|
| `Truncated`, `UnexpectedEnd` | the stream ended in the middle of something |
| `BufferFull` | a fixed output buffer ran out of room |
| `SizeOverflow` | length arithmetic overflowed |
| `MessageSize` | a container of zero-byte elements exceeded `MAX_ZERO_SIZE_ELEMENTS` |
| `TrailingBytes` | `ser::read` finished the object with bytes still left in the buffer |
| `InvalidValue` | a `bool` that is neither 0 nor 1, an unknown variant tag, a duplicate map key, a valueless variant on write |
| `DepthExceeded` | nesting past `MAX_DEPTH` - data-dependent recursion |
| `BadMagic`, `SchemaMismatch`, `PlatformMismatch` | the envelope rejected the stream, see [The Envelope](#the-envelope) |
| `ChecksumFailed`, `Misaligned`, `IoError` | reserved, not produced today |
| `PoolMissing`, `PoolModified`, `DanglingRef`, `DoubleTake` | reserved for object pools, which do not exist yet |

What Works With No Code At All
------------------------------
* **Aggregates** with public fields, nested to any depth. Fields go on the wire in
  declaration order.
* **Scalars**: every arithmetic type and `std::byte`, in native bytes. `bool` is the
  exception - it goes out as an explicit `0`/`1` byte and comes back validated, because a
  `bool` holding anything else is undefined behaviour, not a wrong value. `long double` is
  refused: it has more bytes than it has value, so the padding between them would go on the
  wire as it happened to be and the same number would not give the same stream twice.
* **Enums**, scoped and unscoped, as their underlying type. Changing the underlying type
  changes the format; nothing validates the enumerator list.

  An **unscoped enum with no fixed underlying type** needs one line more. Its valid values
  are those of the smallest bit-field holding its enumerators, and a value off a corrupt
  stream outside that range is undefined behaviour rather than a wrong value - so either
  give it a fixed underlying type, which makes every value of that type a defined one:

  ```cpp
  enum Kind : std::uint32_t { First, Second, Third };   // nothing else needed
  ```

  or declare the range, and anything outside it comes back as `InvalidValue`:

  ```cpp
  enum Kind { First, Second, Third };
  template<> struct ser::enum_range<Kind> {
      static constexpr Kind MIN = First;
      static constexpr Kind MAX = Third;
  };
  ```
* **Fixed arrays**: `T[N]` and `std::array<T, N>`. The length is in the type, so nothing
  about it goes on the wire.
* **Strong integer typedefs**: `STRONG_TYPEDEF_INT` and `STRONG_TYPEDEF_INT_DIMENSIONAL`
  declare their own hook, so a strong typedef - and any struct with a field of one -
  round-trips with nothing written. (`STRONG_TYPEDEF_ID` does not: its value is private
  and minted by `next()`, so an id that has to travel needs a hook of your own.)

With `#include <ser/std/all.hpp>`:

`std::string` and the other `basic_string`s, `std::vector`, `std::map`,
`std::unordered_map`, `std::set`, `std::unordered_set`, `std::pair`, `std::tuple`,
`std::optional`, `std::variant`, `std::monostate`.

With `#include <ser/base/all.hpp>`:

| serializes | refused, with a message naming the replacement |
|---|---|
| `Optional`, `Box`, `MBox` | `Ref` / `CRef` / `MRef` / `MCRef`, `SharedBox`, `BoxOrCRef` |
| `Map`, `HashMap`, `VectorMap`, `StableHashMap` | `RawView`, `ModRawView` |
| `StableVector`, `OwningView`, `SharedView` | `CheckedOkBad` |
| `DynamicBitset`, `Bit256` | |

`OkBad` and `Monostate` need nothing - they are aggregates. `StableObjectPool`,
`ManualLifetimeStorage` and the `MAKE_FLAG_TYPE` flag types have no adapter at all; the
first two hold state a stream cannot describe, and a flag type's mask is private.

`Box` and `MBox` are the plain ones only. Reading either **allocates**, and the one
allocation ser has is `new T` with a default-constructed deleter - so a custom deleter (an
arena, a pool, `malloc`, a C API) is refused: the pairing would be wrong, and the state the
deleter needs to find its way home is not on the wire. If yours is stateless and really does
plain `delete`, say so in one line:

```cpp
template<>
struct ser::box_deleter_is_new_delete<MyDeleter> {
    static constexpr bool VALUE = true;
};
```

A refusal is only visible if you included the header. Leave `<ser/base/all.hpp>` out and a
struct with a `Ref` field gets the generic "looks pointer-like" message instead of the one
that tells you what to do.

Some pairs are deliberately **interchangeable on the wire**: a `Box<T>` stream reads back
into a `T`, a `std::vector<T>` into a `StableVector<T>`, a `std::map` into a
`base::Map` or a `StableHashMap`, a `std::optional` into a `base::Optional`, an
`OwningView` into a `SharedView`. Same bytes, same schema hash - the storage is not part of
the format.

Serializing Your Own Type
-------------------------
Four ways, in the order the library consults them. The first one that answers wins, and a
type with a hook is **never** taken apart field by field - which is what keeps a
container-shaped type of your own from being walked as if it were a container.

**1. `ser::serializer<T>` - for a type you cannot edit.** It outranks everything below.

```cpp
template <>
struct ser::serializer<third_party::Foreign> {
    static constexpr Errc visit(auto& ar, auto& self) { return ar(self.a, self.b); }
};
```

**2. Hooks in the class.** Any one of three shapes - and they may be private, behind
`SER_FRIEND`:

```cpp
struct Versioned {
    std::uint32_t v = 2;
    std::string   payload;

    static ser::Errc serWrite(ser::writer auto& ar, const Versioned& x) {
        return ar(x.v, x.payload);
    }
    static ser::Errc serRead(ser::reader auto& ar, Versioned& x) {
        if (const auto e = ar(x.v); e != ser::Errc::Ok) return e;
        if (x.v != 2) return ser::Errc::InvalidValue;      // your own validation
        return ar(x.payload);
    }
};
```

* `serVisit(ser::reader_or_writer auto& ar, auto& self)` - one function, both directions. The object has to
  exist before it can be filled in.
* `serWrite` + `serRead` - the asymmetric pair, when the two directions genuinely differ.
* `serWrite` + `serMake` - `serMake` **builds** the object and returns it by value, so it
  is the answer for `const` fields, no default constructor, or no move constructor. Read
  the fields with `ser::subMake<F>(ar)`, in braces, so the order is guaranteed.

**3. The same three shapes found by ADL**, for a type in someone else's namespace. Write
both const and non-const overloads of `serVisit`, or the write side will not find it:

```cpp
namespace third_party {
    template <class Ar> ser::Errc serVisit(Ar& ar, Point& p)       { return ar(p.x, p.y); }
    template <class Ar> ser::Errc serVisit(Ar& ar, const Point& p) { return ar(p.x, p.y); }
}
```

**4. Nothing at all** - the aggregate walk.

Four rules the library enforces, each with its own message:

* A hook returns `ser::Errc` (`serMake` returns `T` by value). A hook returning `bool` is
  reported as such, not as "no way to serialize this type".
* A hook is a **template on the archive**. Three concepts name the directions, and spelling
  one is optional but says the intention out loud: `ser::writer auto&` for `serWrite`,
  `ser::reader auto&` for `serRead` and `serMake`, and `ser::reader_or_writer auto&` for the
  symmetric `serVisit`, which serves both. A hook pinned to a concrete archive type is
  silently unreachable, so ser reports it rather than walking past it.
* Write and read must **pair**. A type that can be written and never read back is a bug
  with a use case.
* `serVisit` and `serWrite`/`serRead` on one type is refused - both answer "write this
  one", and nothing says which format was meant. `serMake` next to `serRead` is fine: they
  answer different questions.

The Macros
----------
All of these come with `<ser/ser.hpp>`:

| macro | what it gives you |
|---|---|
| `SER_DESCRIBE(a, b)` | a `serVisit` over exactly those fields - the rest are skipped and come back default-constructed |
| `SER_DESCRIBE_MAKE(T, a, b)` | the same field list as a `serWrite` + `serMake` pair, for a type that must be **built** |
| `SER_MAKE_FROM(T, a, b)` | just the `serMake`, when you write the write side yourself |
| `SER_MAKE_FROM_MEMBERS(T)` | the same without a field list, taken from the walk |
| `*_PAREN` variants | for a type whose braces would reach a `std::initializer_list` constructor |
| `SER_FRIEND` | `friend struct ser::access;` - lets ser see private fields and private hooks |

`SER_DESCRIBE` is also what gives you field **names**, which is what
`SER_TEST_ROUNDTRIP` reports and what `debugHash` mixes in. A described aggregate and the
same aggregate walked automatically produce identical bytes and an identical schema hash,
so adding `SER_DESCRIBE` to a type invalidates nothing that was already written - as long
as you list every field.

```cpp
class Reading {
public:
    Reading(std::uint32_t s, float v): sensor(s), value(v) {}
    SER_FRIEND
private:
    const std::uint32_t sensor;   // const: nothing can fill it in after the fact
    float               value;
    SER_DESCRIBE_MAKE(Reading, sensor, value)
};
```

When The Automatic Walk Needs Help
----------------------------------
Field counting works by probing aggregate initialization, and there are shapes it cannot
count. All of them are compile errors, never wrong bytes.

**A C array field** elides braces, so the probe counts one field per element. Say the
count yourself:

```cpp
struct Frame {
    char tag[4]{};
    int  n{};
    using ser_members = ser::members<2>;   // and the type serializes as any other
};
```

**Private fields** cannot be probed at all. `using ser_members = ser::members<N>;` plus
`SER_FRIEND` is the pair that fixes it - the alias may be private too. A wrong `N` is a
compile error in the bindings ladder, so the format cannot drift silently.

**A base class** is an element of aggregate initialization and not of a structured binding.
A base with no data members of its own is settled by `ser_members<N>`; one that carries
data cannot be decomposed at all and needs a hook.

**A reference field** is refused outright, in every direction: reading has to overwrite the
object and a reference can never be rebound. Hold the value, or leave the field out with
`SER_DESCRIBE`.

**A `const` field or a bit-field** cannot be written through a reference, so such a type is
**built** rather than filled. As a whole object it reads fine (`ser::read<T>` builds it);
as a *field* of an object being filled it needs the enclosing type to have a `serMake`, or
`SER_DESCRIBE_MAKE`. A bit-field widens to its declared type on the wire.

**More than 64 members** is past the limit of the structured-bindings ladder. Split the
type, or give it a `serVisit`.

What ser Refuses
----------------
Not "unsupported for now". Each of these has no meaning outside the writing process, and
guessing would be silent data corruption rather than an error:

| refused | write this instead |
|---|---|
| raw pointer | the value itself, `std::optional<T>`, or an index into a table you own |
| `char*` / `const char*` | `std::string`, or `char[N]` for a fixed buffer |
| `void*`, function pointer, pointer to member | a tag or an index you map back after reading |
| reference, `std::reference_wrapper` | the referenced value, or an index |
| union | a tag next to the payload and a `serVisit` that switches on it |
| `std::vector<bool>` | `std::vector<std::uint8_t>`, or `std::bitset<N>` |
| `std::string_view`, `base::RawView` | the owning type - a view cannot be read back into |
| `std::shared_ptr`, `std::unique_ptr` | not supported yet; `base::Box` / `base::MBox` do work |
| a polymorphic `Box<Base>` | a tag plus a serializer of your own that switches on it |
| a `Box` / `MBox` with a custom deleter | the value, put back where it belongs after reading - or opt in with `ser::box_deleter_is_new_delete` |

Archives: Several Messages In One Buffer
----------------------------------------
`ser::read` always starts at the front of the span it is given and never says how far it
got. An archive does say, so reading several appended messages is one archive rather than
several calls to `ser::read`:

```cpp
std::vector<std::byte> buf;
ser::writeOrPanic(buf, header);       // buf: [header]
ser::writeOrPanic(buf, payload);      // buf: [header][payload]

ser::in ar{ std::span<const std::byte>{ buf } };
if (const auto e = ar(header); e != ser::Errc::Ok) return { e, ar.position() };
if (const auto e = ar(payload); e != ser::Errc::Ok) return { e, ar.position() };
const std::size_t consumed = ar.position();     // where the next message begins
```

`ar.position()`, `ar.size()`, `ar.avail()` and `ar.reset(p)` are all public. `ser::out`
works the same way and takes several objects at once - `ar(a, b, c)` stops at the first
failure and returns that argument's code. Call `ar.finish()` when a write archive is done;
without pools it folds away to nothing.

The Envelope
------------
Thirty-two bytes in front of the payload, so that a stream from another build, another byte
order or another schema is refused instead of being interpreted as data. It is **off by
default**, so a plain `ser::write` is exactly the payload and not one byte more.

```cpp
constexpr ser::options opt{ .header = true, .user_magic = 0xD0C5 };

ser::writeOrPanic(buf, cfg, opt);
const auto r = ser::read<Config>(buf, opt);
```

What it checks, in this order, because the order is the diagnosis: the 32 bytes are there
(`Truncated`), the magic and your `user_magic` match (`BadMagic`), the platform flags match
- byte order, pointer width (`PlatformMismatch`), the sizes add up
(`SizeOverflow`/`Truncated`), and finally the schema hash matches the type being read
(`SchemaMismatch`).

`ser::peekHeader(bytes, user_magic)` reads the envelope without committing to a type, so a
caller holding several possible types can compare `h.schema_hash` against
`ser::schemaHash<T>()` for each of them and pick. `payload_crc` is reserved and written as
zero - there is no checksum yet.

Schema Hash
-----------
`ser::schemaHash<T>()` is `consteval` and returns one 64-bit number meaning "this is the
format I write". It hashes the **wire**, field by field: padding and alignment stay out, so
a layout change does not invalidate a stream that is still readable.

The number is **not portable**: it also covers the byte order and the pointer and length
widths, because its only job is to reject a stream this build cannot read. In a module that
stores ser data, test it in one of two ways:

* that two types have the same format - this holds on every platform:

  ```cpp
  static_assert(ser::schemaHash<Described>() == ser::schemaHash<Twin>());
  ```

* that the format of a stored type did not change by accident - pin the literal read back
  with `ser::peekHeader`, the way `debug_info_test` does for `.di` files. The literal is
  only valid for one platform. When it changes on purpose, update it: every file written
  before is now `SchemaMismatch`.

A type whose format ser cannot see - a hand-written `serWrite`, a class with private
members - hashes as `"hook"` plus `sizeof` and `alignof`, which two unrelated types can
share. Three ways to say what it really writes, in the order they are consulted:
`ser::config<T>::schema_id`, a `ser::schema<T>` specialization, or an in-class
`using ser_wire_as = W;` (with an optional `ser_schema_tag`). `ser_wire_as` says "the wire is this type";
`ser_schema_tag` is a discriminator mixed into the hash, so two strong typedefs over one
integer do not collide.

Field names are absent from `schemaHash` on purpose. They go into `ser::debugHash<T>()`,
which is diagnostics only: equal `schemaHash` with different `debugHash` is exactly
"somebody renamed a field".

Testing Your Format
-------------------
Two fields of the **same type** swapped in a `SER_MAKE_FROM` list compiles perfectly and
writes the wrong bytes. No hash can see it, so there is a round-trip check instead:

```cpp
CHECK(SER_TEST_ROUNDTRIP(Vec3{ 1, 2, 3 }));
```

It writes the sample, reads it back, compares field by field and prints which field came
back different - by name, when `SER_DESCRIBE` gave it one. With `SER_MAKE_FROM_MEMBERS`,
which has no field list to check anything against, this is a condition of use rather than
a suggestion. The same goes for a type that pairs `serVisit` with `serMake`: that pair is
allowed - it is how a type that cannot be filled in place is read - but the two are separate
descriptions of one format, and nothing but a round-trip can tell you they still agree.

The Wire Format At A Glance
---------------------------
No padding, no alignment, no field names, no type tags. Nothing is packed and nothing is
compressed.

| | |
|---|---|
| scalar | its native bytes |
| `bool` | one byte, `0` or `1`, validated on read |
| enum | its underlying type |
| fixed array | the elements, each through full dispatch |
| aggregate | its fields, in declaration order |
| container | `u64` length, then the elements (a map: each key followed by its value) |
| `std::string` | `u64` length, then the characters - no terminator, embedded `\0` survives |
| optional / `MBox` | one presence byte, then the value if present |
| variant | `u64` tag, then that alternative; `monostate` writes nothing |
| empty type | zero bytes |

The length prefix is `u64` on every platform, so a stream does not depend on the writer's
`size_t`. Note that iteration order of the **unordered** containers is unspecified: one map
written twice gives the same bytes, but two *equal* maps need not. Compare a round-trip
element by element, not byte for byte, and reach for `std::map` when a payload is going to
be signed or content-addressed.

Limits And Knobs
----------------
`ser::config_global` holds the policy: `MAX_DEPTH` is 256 (data-dependent recursion is a
stack overflow on write and an attack on read), `MAX_ZERO_SIZE_ELEMENTS` is 2^28, and the
length prefix type is `u64`.

Every variable-length read validates the length before allocating anything: a corrupt
length gives you `SizeOverflow` or `Truncated`, never a huge allocation. There is no fixed
ceiling on the element count - the stream itself has to hold the bytes, which is a tighter
bound than any constant and lets a container as large as your data reads.

Elements that occupy **no bytes** are the exception: the stream carries no evidence about
their count, so `MAX_ZERO_SIZE_ELEMENTS` bounds them and the error is `MessageSize` - on
write as well as read, so nothing can be written that cannot be read back.

C++23 Today, C++26 Later
------------------------
ser is a C++23 library and behaves identically whether or not you build with a later
standard.

Field enumeration is the only place where the standard version is visible. Today it is a
structured-bindings ladder - the arity table lives in `base/preproc/ladder.hpp`, the field
count probe in `base/comptime/aggregate_arity.hpp` and the rungs that walk members in
`base/comptime/member_walk.hpp`, since none of them says anything about serialization - and
everything in
[When The Automatic Walk Needs Help](#when-the-automatic-walk-needs-help) follows from
that: the 64-member limit, the C-array counting problem, and `ser_members<N>` for private
fields.

C++26 removes those causes - P1061 lets a structured binding introduce a pack, and P2996
reflection can enumerate members directly, private ones included. Neither is used yet: no
released compiler supports both. When they land, they land behind the same public API - the
`ser_members` declarations become unnecessary rather than wrong.

Nothing else in the library waits on C++26.

Compilers
---------
| compiler | status |
|---|---|
| GCC 14 (`-std=c++23`) | works - the toolchain this repository builds with, and what runs the module's tests |
| Clang 19 (`-std=c++23`) | works - both test files compile clean and produce byte-identical streams and the same `schemaHash` |
| anything below C++23 | `#error "ser: C++23 is required"` |
| MSVC | the guards are in place - `/std:c++latest`, `/Zc:__cplusplus` and `/Zc:preprocessor` are required and a missing one is an `#error` - but no build here exercises it |

C++23 is required for real reasons, not for tidiness: `std::expected` is what `ser::result`
is, and `if !consteval` is what lets the bulk-copy paths exist next to the constant-evaluable
ones.

Limitations
-----------
* **No object graphs.** Pools for `Box`/`Ref` are designed for but not implemented: shared
  ownership is refused, and two pointers to one object are written as two objects.
* **No polymorphism.** A `Box<Base>` holding a `Derived` cannot be written - the stream
  would have to name the type to allocate. Write a tag and switch on it.
* **No schema evolution.** There are no optional or defaulted fields; adding one changes
  the format. The envelope's job is to *detect* that, not to survive it. If you need
  versioning, write a version field and branch on it in `serRead`.
* **No varints, no compression, no checksum.** `payload_crc` is reserved and written as
  zero.
* **No zero-copy reads.** Every read allocates its own storage; views that do not own their
  bytes are refused.
* **No `chrono` adapters** yet, and no `std::unique_ptr`/`std::shared_ptr`.

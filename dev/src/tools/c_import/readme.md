# `duck c-package` and `duck_c_import`

`duck c-package` generates a Duckling package that binds a C library. The package can be
committed and published like any other. `duck_c_import` is the libclang-based translator behind
it.

```bash
duck c-package new sdl3 --pkg-config sdl3 --header SDL3/SDL.h --include 'SDL_*'
duck -C app add --local ../sdl3 sdl3
```

```duck
import sdl3.*;
```

## What it is for, and what it is not

The tool brings a C library into Duckling as a package that other projects depend on. The
package holds no paths from the machine it was made on:

- The recipe (`c-package.yaml`) holds header names, pkg-config package names and paths relative
  to the package.
- The manifest names the libraries (`metadata.external-libs`) without locating them. duck finds
  their files when the package is built, on the machine that builds it.
- The verification program, whose sizes are specific to the target, is not part of the package.

It is not a generator of idiomatic wrappers. The names and types are C's. A hand-written SDL
package could build on the generated one.

## Where a package works

The bindings are made for one target and one version of the library, both named in their banner
(`from uv.h (libuv 1.53.0)`, `Target: x86_64-pc-linux-gnu.`). The target is the one the system C
compiler builds for (`gcc -dumpmachine`); record layouts, the width of `long` and the signedness of
`char` come from it. A package works on another machine when:

- **The architecture and OS are the same.** Bindings for several targets in one package need
  target conditions in Duckling (#4142).
- **The library has the same name.** `external-libs` holds the name found where the package was
  generated, and distributions differ: Arch's `lua` is `lua5.4` on Debian, Ubuntu and Alpine.
- **The library is at least as new as the headers.** An older one links, but structs and constants
  that changed in between are read wrongly. Generate a package you publish against the oldest
  release it supports.
- **A shared library is found.** duck links `lib<name>.a` before `lib<name>.so` (#4140), and a
  static library's own dependencies are not linked.

Packages for 23 libraries generated on Arch Linux (glibc) were used unchanged on Ubuntu 24.04,
Ubuntu 26.04 and Alpine 3.24 (musl). All of them built and ran except lua, for its library name,
and curl on Ubuntu, whose `libcurl.a` was picked. musl's libc types have other names than glibc's
but the same sizes for these libraries; nothing guarantees that in general.

Finding the library on the consumer's machine, checking its version and target, and building it
from source belong to build scripts, which the generator will write once they exist (#4200).

## Commands

```bash
duck c-package new <path> --header <h>... [--pkg-config <pkg>]... [--cflag <flag>]...
                          [--library <lib>]... [--include <glob>]... [--exclude <glob>]...
                          [--name <name>] [--no-verify]
duck c-package regen [<path>] [--no-verify]
duck c-package check [<path>]
```

- `--header` takes a path, or a name found on the include path (`SDL3/SDL.h`).
  Repeat it for a library with several headers (`--header lua.h --header lauxlib.h --header
  lualib.h`). They are read as one translation unit into one module, and what each header
  includes is translated too, within its scope:
  - a path claims its whole directory;
  - a name with a directory (`SDL3/SDL.h`) claims that directory, so an umbrella header brings
    all of it;
  - a bare name (`zlib.h`) claims its own file plus what it includes from below its directory.

  Records those declarations need are pulled in from wherever they are defined. A header vendored
  inside the package (`--header sdl3/include/SDL.h`) keeps the package independent of the headers
  installed on the machine.
- `--library` takes `-l<name>`, `-L<dir>`, or a path to a `lib<name>.a`, `.so` or `.dylib`.
  pkg-config's `--libs` are added to them. They become bare names in `metadata.external-libs`, and
  directories in `metadata.external-lib-paths`: relative ones when they are inside the package,
  none for the directories duck searches anyway, and a warning for any other absolute directory.
  Object files are rejected; put them in an archive.
- `--include` and `--exclude` are name globs. Without `--include`, every declaration of the
  requested headers is translated.
- Paths given to `new` are relative to the current directory; the recipe stores them relative to
  the package.
- Unless `--no-verify` is given, the package is checked after it is written. A throwaway program
  depending on it is built and run. It compares `size_of`/`alignment_of` of every class with what
  clang reported, so a layout mistake fails generation instead of corrupting memory at run time.
- `check` regenerates into a temporary directory and fails when the bindings or the library keys
  of the manifest differ from what the recipe gives, without changing anything. It compares text,
  so on a machine with another version of the library it reports a difference.

The generated package:

```
sdl3/
  c-package.yaml         the recipe; edit it, then run `regen`
  quackconfig.yaml       an ordinary manifest; `regen` rewrites only the library keys
  src/
    src.dk               re-exports the bindings; written once, then yours
    generated/           rewritten by every regen
      generated.dk       banner (the directory's main module)
      sdl3.dk            the bindings
```

```yaml
# c-package.yaml
headers:
- SDL3/SDL.h
pkg-config:
- sdl3
include:
- SDL_*
```

```yaml
# quackconfig.yaml
metadata:
  name: sdl3
  version: '3.4.16'              # pkg-config --modversion, when it is a plain x.y.z
  external-libs: [SDL3]
```

The version is set by `new` and is yours afterwards. A regen with an unchanged recipe produces
byte-identical files.

## What C becomes

| C | Duckling |
|---|---|
| `int32_t`, `unsigned`, `float`, ... | the exact-width `iN` / `uN` / `fN`; typedefs resolve to them |
| `bool` | `bool` |
| `char` | `i8` or `u8`, per the target's signedness, because Duckling's `char` is unsigned; `char *` is `cptr char` |
| `enum E { A = 1 }` | `const A: <underlying> = 1<suffix>;`, and `E` itself is its underlying integer |
| `#define X 0x20u`, `#define Y (1 << 3)` | a `const`, typed and valued by clang (see below) |
| struct with its natural layout | `extern("C") class` with the same fields |
| forward-declared or incomplete struct | an opaque handle, `extern("C") class X { _opaque: u8; }`, used as `cptr X` |
| complete struct only reached through pointers (`mpz_t`'s `__mpz_struct`) | still gets its layout, so it can be allocated |
| record with no possible layout (16-byte alignment) that nothing needs by value | an opaque handle, with the reason in its comment |
| union, packed, bitfield or over-aligned record | a **layout blob** plus generated accessors (see below) |
| anonymous struct member | its fields are spliced into the outer class |
| anonymous union member | a blob-typed field `__anonN`, plus accessors on the outer record |
| `T a[N]`, `T a[N][M]` | `T[N]`, `T[N*M]` (flattened: same layout, no index order to guess) |
| `void *`, function pointers | `cptr u8` |
| field or parameter named like a keyword (`type`, `in`) | renamed `type_`, `in_` (only positions are linked) |
| function named like a keyword (`match`) | `@c_symbol_name("match") fundecl match_(...)` |
| record named like a function (`struct stat` and `stat()`) | the record is renamed `stat_struct` |

### Layout blobs and their accessors

A record Duckling cannot lay out itself becomes storage of exactly the right size and alignment:
`_storage: uK[N]`, where `K` is the alignment and `N` is size / alignment. Its members are reached
through generated functions:

- **An aligned member** gets a pointer view, `R_as_f(p) -> cptr F`, such as
  `fun SDL_Event_as_key(p: cptr SDL_Event) -> cptr SDL_KeyboardEvent`. At offset 0 it is just a
  cast; elsewhere it is `&bytes[offset] as cptr F`.
- **A misaligned member (packed) or a bitfield** gets a getter and setter, `R_get_f(p)` and
  `R_set_f(p, value)`, which assemble it from single bytes, with sign extension for signed
  bitfields.

A blob is classified as integers when it is passed by value. That matches C unless the record is
16 bytes or smaller and holds a floating-point member, so functions taking or returning such a
record by value are skipped.

### Macros

Every object-like macro of the requested headers is evaluated by clang itself: the translator
compiles `static const __auto_type __duck_m_X = (X);` for each one. So `0x20u` becomes `u32`,
`SDL_UINT64_C(0x20)` becomes `u64`, and `(1 << 3)` works. Macros that do not evaluate to a number
(include guards, string literals, attribute macros) are left out and only counted.

### Doc comments

A C doc comment becomes `#` lines above what it documents: a class, field, `fundecl`, constant or
blob accessor. An enum's own comment stands above its first constant, followed by a blank line.
Doc comments are Doxygen's forms (`/** */`, `///`, `/*! */`, `//!` and the trailing `/**< */`);
plain `/* */` comments are left out, since they are as often license headers or disabled code.
The text is kept as written, Doxygen commands such as `\param` included.

libclang attaches no comments to macros, so a macro's is read from the header: one trailing its
`#define` line, or one ending on the line right above it.

## What is skipped

Skipped declarations are listed, with the reason, at the top of the generated module.

| Construct | Why |
|---|---|
| variadic functions (`SDL_Log`) | one declaration per call signature would be needed (#3271) |
| `static` / `static inline` functions | there is no symbol to link against |
| a small blob with a float member, passed by value | see [Layout blobs](#layout-blobs-and-their-accessors) (#3647) |
| `long double`, `__int128` | no Duckling mapping (#1498) |
| a flexible array member (`int rest[];`) | the record becomes a blob and that member gets no accessor, since zero-length arrays are rejected (#3647) |
| global variables | not supported |

Other gaps have workarounds rather than being skipped:

- **Callbacks** are `cptr u8`. You can pass one through from C, or pass null, but not call it or
  build one (#3650).
- **Anonymous members** are spliced in or reached through accessors, not addressed as C writes
  them (#3648).
- **Names that would be ambiguous in Duckling.** A field or parameter named like a module-level name
  (`addrinfo: cptr addrinfo`) gets a `_` suffix, and generated functions use `__dk_`-prefixed
  locals (#2135).
- **16-byte-aligned records** have no storage type, since `extern("C")` classes reject `u128`
  (#1498). One that nothing needs by value becomes an opaque handle; otherwise it is skipped.

## Backends

duck resolves `metadata.external-libs` when the package is built (#3954) and links them on the
native backend. The DVM does not load `external-libs` yet (#4144), so a package that links a library
builds on the native backend only for now, and generation checks it there. On macOS, `c-package`
passes the SDK (`xcrun --show-sdk-path`) to libclang as `-isysroot`.

## How it works

```
duck c-package (Rust, src/duck/src/lib/quackpack/subcommands/c_package)
  c-package.yaml -> pkg-config, library names -> request.json
    -> duck_c_import (C++, this directory) -> *.dk + report.json
  -> src/generated/, quackconfig.yaml, src.dk -> verification program
```

`duck_c_import <request.json> [<report.json>]` runs as a pipeline over plain data models:

1. `tu_reader` (the only libclang user, `src_private/`) parses one synthesized translation unit that
   includes every requested header. It returns a `CModel` (`c_model.hpp`) with clang's exact sizes,
   alignments and field offsets, and evaluates the macros. A declaration counts as coming from the
   requested headers when it is in the directory of a header given by path, or in the directory
   named in `SDL3/SDL.h`. A bare name like `math.h` claims its own file and the files it includes
   from below its directory, directly or not (glibc declares most of `math.h` in `bits/`), rather
   than the whole system include directory.
2. `lower` decides what each declaration becomes, as a `DkModule` (`dk_model.hpp`). It keeps the
   requested declarations and every record they need, checks whether Duckling's natural layout
   reproduces each record, and falls back to blobs or skips with a reason.
3. `emit` writes the module text and the layout check, which only the verification program uses.

`lower` and `emit` never touch clang, so `tests/c_import_lower_test.cpp` tests them on hand-built
models. Everything that depends on the machine (libclang, system headers and libraries) is tested
in `integration_tests/duck/c-package/`:

- the bindings generated from `tests/headers/sdl_like.h`, a fixture shaped like SDL3's API, are
  compared against `expected_bindings.txt`;
- that package, with the header and library vendored inside it, is verified, run, published
  through git and used from another directory, and holds no absolute path;
- `math.h` and `-lm` are found by bare names, and zlib through pkg-config;
- regeneration and `check` are tested, and so is bad input;
- SDL3 itself, when pkg-config finds it.

`examples/sdl3_demo.sh` builds and runs an SDL3 program (`--window` for a real window).

libclang is a required build dependency, found next to the LLVM the build uses
(`dependencies/Clang.cmake`).

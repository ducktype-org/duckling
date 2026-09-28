# `duck translate-c` and `duck_c_import`

`duck translate-c` generates a Duckling package that binds a C library installed on this machine.
You then use it like any other local dependency. `duck_c_import` is the libclang-based translator
behind it.

```bash
duck translate-c new sdl3 --pkg-config sdl3 --header SDL3/SDL.h --include 'SDL_*'
duck -C app add --local ../sdl3 sdl3
```

```duck
import sdl3.*;
```

## What it is for, and what it is not

The tool automates bringing a C library that is *already installed here* into Duckling. The
generated package is a **local, machine-specific build artifact**, like a `build/` directory:

- Its manifest records what was found on this machine: pkg-config output (`-L` paths), the
  resolved soname for the DVM, and the library version.
- Its bindings record the layouts of the headers installed here, and the target's `char`
  signedness.

So **do not commit, publish or share a generated package.** Each machine, and each CI job, runs
`duck translate-c new ...` again, or `duck translate-c regen` on a package that already exists.
Every generated file says this in its banner.

It is not a generator of idiomatic, publishable wrappers. The names and types are C's. A
hand-maintained SDL package for others to depend on would be a separate package, which could
build on the generated one.

## Commands

```bash
duck translate-c new <path> --header <h>... [--pkg-config <pkg>]... [--cflag <flag>]...
                            [--library <lib>]... [--include <glob>]... [--exclude <glob>]...
                            [--name <name>] [--no-verify]
duck translate-c regen [<path>] [--no-verify]
```

- `--header` takes a path, or a name found on the include path (`SDL3/SDL.h`).
- `--library` takes a path to a `.o`, `.a` or `.so`, or `-l`/`-L` flags. pkg-config's `--libs` are
  added to them.
- `--include` and `--exclude` are name globs. Without `--include`, every declaration of the
  requested headers is translated.
- Unless `--no-verify` is given, the package is checked after it is written. A throwaway program
  depending on it is built and run on the native backend, and on the DVM when the package has
  shared objects to load. The program compares `size_of`/`alignment_of` of every class with what
  clang reported, so a layout mistake fails generation instead of corrupting memory at run time.

The generated package:

```
sdl3/
  quackconfig.yaml       metadata (regenerated) + c-bindings (the recipe)
  src/
    src.dk               re-exports the bindings; written once, then yours
    generated/           rewritten by every regen
      generated.dk       banner (the directory's main module)
      sdl3.dk            the bindings
      layout_check.dk    c_layout_mismatch(), used by the check
```

```yaml
metadata:
  name: sdl3
  version: 3.4.16                # pkg-config --modversion, when it is a plain x.y.z
  links: -lSDL3                  # native linker
  dvm-shared-libs:               # what the DVM dlopens instead
  - libSDL3.so.0
c-bindings:
  headers: [SDL3/SDL.h]
  pkg-config: [sdl3]
  include: ['SDL_*']
```

Change `c-bindings`, then run `regen`. `metadata` and `src/generated/` are rewritten every time,
so the build itself never needs pkg-config or libclang. A regen with an unchanged recipe produces
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
| forward-declared or incomplete struct, or one only ever used through a pointer | an opaque handle, `extern("C") class X { _opaque: u8; }`, used as `cptr X` |
| union, packed, bitfield or over-aligned record | a **layout blob** plus generated accessors (see below) |
| anonymous struct member | its fields are spliced into the outer class |
| anonymous union member | a blob-typed field `__anonN`, plus accessors on the outer record |
| `T a[N]`, `T a[N][M]` | `T[N]`, `T[N*M]` (flattened: same layout, no index order to guess) |
| `void *`, function pointers | `cptr u8` |
| field or parameter named like a keyword (`type`, `in`) | renamed `type_`, `in_` (only positions are linked) |
| record named like a function (`struct stat` and `stat()`) | the record is renamed `stat_struct` |

### Layout blobs and their accessors

A record Duckling cannot lay out itself becomes storage of exactly the right size and alignment:
`_storage: uK[N]`, where `K` is the alignment and `N` is size / alignment. Its members are reached
through generated functions:

- **A union member at offset 0** gets a pointer view, which is just a cast:
  `fun SDL_Event_as_key(p: cptr SDL_Event) -> cptr SDL_KeyboardEvent`.
- **Any other member** gets a getter and setter, `R_get_f(p)` and `R_set_f(p, value)`. An aligned
  member goes through a view class `R__view_f` that places it at its offset. A misaligned member
  (packed) or a bitfield is assembled from single bytes, with sign extension for signed
  bitfields.

Getters and setters are used instead of pointers because the DVM cannot yet take the address of a
place reached through a `cptr` (#3662).

A blob is classified as integers when it is passed by value. That matches C unless the record is
16 bytes or smaller and holds a floating-point member, so functions taking or returning such a
record by value are skipped.

### Macros

Every object-like macro of the requested headers is evaluated by clang itself: the translator
compiles `static const __auto_type __duck_m_X = (X);` for each one. So `0x20u` becomes `u32`,
`SDL_UINT64_C(0x20)` becomes `u64`, and `(1 << 3)` works. Macros that do not evaluate to a number
(include guards, string literals, attribute macros) are left out and only counted.

## What is skipped

Skipped declarations are listed, with the reason, at the top of the generated module.

| Construct | Why |
|---|---|
| variadic functions (`SDL_Log`) | one declaration per call signature would be needed (#3271) |
| `static` / `static inline` functions | there is no symbol to link against |
| functions named like a Duckling keyword (`match`, `in`) | the name is the linked symbol (#3649) |
| a small blob with a float member, passed by value | see [Layout blobs](#layout-blobs-and-their-accessors) (#3647) |
| `long double`, `__int128` | no Duckling mapping (#1498) |
| a flexible array member (`int rest[];`) | the record becomes a blob and that member gets no accessor, since zero-length arrays are rejected (#3647) |
| global variables | not supported |

Other gaps have workarounds rather than being skipped:

- **Callbacks** are `cptr u8`. You can pass one through from C, or pass null, but not call it or
  build one (#3650).
- **Pointers between records that point back to each other** (`struct SDL_AssertData *next`)
  are `cptr u8`, because such a pointer cycle cannot be laid out yet (#2616). The class comment
  says which ones.
- **Anonymous members** are spliced in or reached through accessors, not addressed as C writes
  them (#3648).
- **A `void` return** is left out of the declaration, since an explicit `-> void` miscompiles (#3646).

## Backends

`metadata.links` goes to the native linker. `metadata.dvm-shared-libs` becomes `ffi object`
entries in the DVM bytecode, which the VM `dlopen`s:

- a library found in a system directory is named by its soname (`libSDL3.so.0`);
- one found only through `-L` is named by its full path;
- on macOS a library is always named by its full path, since a Mach-O library has no soname and
  `dlopen` does not search Homebrew's prefix;
- `.o` and `.a` libraries cannot be loaded by the DVM, and a warning says so.

A package that has only static artifacts builds on the native backend only. On macOS,
`translate-c` also passes the SDK (`xcrun --show-sdk-path`) to libclang as `-isysroot`.

## How it works

```
duck translate-c (Rust, src/duck/src/lib/quackpack/subcommands/translate_c)
  recipe -> pkg-config, library and soname resolution -> request.json
    -> duck_c_import (C++, this directory) -> src/generated/*.dk + report.json
  -> quackconfig.yaml, src.dk -> verification program on each backend
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
3. `emit` writes the module text and the layout check.

`lower` and `emit` never touch clang, so `tests/c_import_lower_test.cpp` tests them on hand-built
models. Everything that depends on the machine (libclang, system headers and libraries) is tested
in `integration_tests/duck/translate-c/`:

- the bindings generated from `tests/headers/sdl_like.h`, a fixture shaped like SDL3's API, are
  compared against `expected_bindings.txt`;
- that package is verified and run on both backends;
- `math.h` and `-lm` are found by bare names;
- regeneration is checked, and so is bad input;
- SDL3 itself, when pkg-config finds it.

`examples/sdl3_demo.sh` builds and runs an SDL3 program on both backends (`--window` for a real
window).

libclang is found next to the LLVM the build uses (`dependencies/Clang.cmake`). Without it, the
tool is not built and the integration tests are disabled.

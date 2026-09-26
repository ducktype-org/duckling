# `duck_c_import`

Generates a source-only Duckling package from C headers, so a C library can be imported and used
like any Duckling library. Usually invoked through `duck translate-c`, which is a thin wrapper
around this binary.

```bash
duck translate-c mylib --header mylib.h --library /abs/path/mylib.o
```

That writes

```
mylib/
├── quackconfig.yaml      # metadata.name/version/links/dvm-shared-libs
└── src/
    ├── src.dk            # banner only; a directory without a main module file is not a module
    └── mylib.dk          # the bindings
```

which is consumed as an ordinary path dependency:

```bash
duck -C app add --local ../mylib mylib
```

```duck
import mylib.mylib.*;
```

## Checking the output

After writing the package, the translator compiles it (as a static library, since bindings have
no `main`) and refuses to report success if it does not build. A failure prints the compiler's
diagnostics and the exact command to reproduce them, and leaves the package in place to inspect:
that combination is a bug in the translator, not in the user's code. `--no-verify` skips the
check and `--duckc <path>` picks the compiler, which otherwise comes from `$PATH`.

## How it works

Every requested header is `#include`d from one synthesized translation unit, so clang owns the
include graph, declaration dedup and forward-declaration resolution. The whole translation unit is
emitted, like `zig translate-c` does; clang's own predefined macros are the one exception, since
they belong to no header.

libclang is confined to `src_private/c_import_private/` — `clang_raii.hpp` owns every handle and
`tu_reader.cpp` does the walking. Everything downstream operates on `c_decls.hpp`, a plain data
model, so the type mapper and the emitters are pure functions that test without clang in the loop.

## What is skipped

The generator only emits what `QueryCAbiTypeOf`
(`src/compiler/core/tsl/src/tsl/c_abi_converter.cpp`) accepts, so a mistake is a compile error
rather than a mis-linked call. Anything else is skipped with a `#` comment naming the reason:

| Construct | Why |
|---|---|
| unions, bitfields, packed records | no C ABI support in `src/common/abi` |
| a record with any skipped field | the layout could not be reproduced |
| flexible array members | zero-length arrays are rejected |
| `__int128`, `long double` | no Duckling mapping |
| variadic functions | one declaration per concrete call signature is needed; a template is emitted to copy |
| `static` / `static inline` | no external symbol to link against |
| C identifiers that are Duckling keywords | the symbol name *is* the linked identifier, so renaming would break the link |
| macros that are not a single numeric literal | no general C expression evaluation |

Three mappings are worth knowing about:

- A C function returning `void` is emitted without a return type. An explicit `-> void` on an
  `extern("C") fundecl` miscompiles and crashes when called (#3646).

- C `char` becomes `i8`/`u8` rather than Duckling `char`, which lowers to an unsigned 8-bit
  integer — a signed `char` argument would otherwise be zero-extended instead of sign-extended.
  `char *` still becomes `cptr char`, where no promotion applies.
- `void *`, function pointers and pointers to opaque or skipped records all become `cptr u8`.
  An opaque record gets no class of its own, because an `extern("C")` class with no fields is
  rejected by the compiler.

A field name is not linked - the C ABI places fields by position - so a field called `type` is
renamed to `type_` rather than costing the whole record. A *function* name is the linked symbol,
so a keyword there means the declaration has to be skipped instead.

Which records get a class is decided before anything is written. That is what lets a type
reference be trusted: nested and anonymous definitions are found even though they are not
top-level declarations, and a record skipped later in the file is already known to be skipped
when an earlier declaration points at it.

## One module or many

By default every declaration lands in a single module, so the import site is
`import mylib.mylib.*;`. With `--split` each header becomes its own module instead:

```
shapes/src/
├── src.dk
├── mylib.dk            # module shapes.mylib
├── mylib_extra.dk      # module shapes.mylib_extra
└── shapes.dk           # gathers both, see below
```

A module naming a record owned by another one imports it. A sibling module has to be reached
through the package name, and the import binds its last segment:

```duck
import shapes.mylib;
using mylib.*;
```

The aggregate module (`shapes.dk`) gathers every split module, but `using` binds locally rather
than re-exporting, so for now each module has to be imported individually. It is dropped when a
header stem already claims the package name.

## Backends

`metadata.links` is passed to the linker verbatim and is native-only. For the DVM, pass
`--dvm-shared-lib` as well: it becomes `metadata.dvm-shared-libs`, which reaches the VM as an
`ffi object` entry that is `dlopen`ed at load time. A package declaring both works on either
backend.

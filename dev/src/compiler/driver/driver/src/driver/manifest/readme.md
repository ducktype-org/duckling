# Manifest Compilation (`manifest.json`)

The manifest describes what the compiler should build: a list of packages
(together with their dependencies) and a list of tasks to perform on those
packages.

## Top-level structure

```json
{
  "packages": [ /* RawPackageInfo, ... */ ],
  "tasks":    [ /* RawTask, ... */ ]
}
```

Both fields are **required**. Unknown fields produce a diagnostic.

## `packages[]` — package entry (`RawPackageInfo`)

| field          | type     | required | description |
|----------------|----------|----------|-------------|
| `id`           | string   | yes      | Package ID — unique identifier within the manifest (used by dependencies and tasks). |
| `name`         | string   | yes      | Import name of the package (used when resolving imports in source code). |
| `path`         | string   | yes      | Path to the package's source root (root module). |
| `version`      | string   | no       | version string. |
| `features`     | string[] | no       | Feature flags |
| `dependencies` | obj[]    | no       | Dependencies — see below. |

Each dependency (`RawDependencyInfo`):

| field   | type   | required | description |
|---------|--------|----------|-------------|
| `id`    | string | yes      | ID of the dependency package. **Must match a package `id` declared in `packages[]`**. |
| `alias` | string | no       | Name used to import the package in code. Defaults to the target package's `name`. |

Validation rules (`verify`):
- `packages` must not be empty.
- Package `id`s must be unique.
- Package `name`s must also be unique.
- Every `dependency.id` must exist in `packages[]`.
- Within a single package: no duplicate `dependency.id` and no duplicate effective aliases.

## `tasks[]` — compilation tasks (`RawTask`)

Currently a single task type: package compilation. Common fields:

| field      | type   | required | description |
|------------|--------|----------|-------------|
| `package`  | string | yes      | ID of a package from `packages[]` to build. |
| `strategy` | string | yes      | `"dvm_exe"` \| `"dvm_lib"` \| `"native"` \| `"obj"` \| `"lib"`. |

Remaining fields depend on `strategy`:

### `strategy: "dvm_exe"` — DVM bytecode executable
| field                 | required | description |
|-----------------------|----------|-------------|
| `output_file`         | yes      | Output file stem. |
| `dvm_linking_options` | no       | An object — see below. |

### `strategy: "dvm_lib"` — DVM bytecode library
| field                 | required | description |
|-----------------------|----------|-------------|
| `output_file`         | yes      | Output file name. |
| `dvm_linking_options` | no       | An object — see below. |

`dvm_linking_options` (both DVM strategies) — config baked into the generated
DBC so the DVM can run it:
- `shared_libraries` (string[], optional) — shared libraries the VM has to load
  to run the code. A bare name (e.g. `"libm.so.6"`) is searched in the system
  library paths, a path is loaded as given.
- `link_libraries` (string[], optional) — paths of `.dbc` libraries to link into
  the final output.

Unlike `linking_options` / `archive_options`, there is no short (string) form —
both fields are lists, so the object form is always required.

> **IMPORTANT — linking against other DVM packages.** As with the `native`
> strategy, listing a package in `dependencies` is **not enough**. You must
> pass the path to the `.dbc` file produced by its `dvm_lib` task inside
> `link_libraries`. The driver does not resolve those paths automatically.

### `strategy: "native"` — native executable (LLVM)
| field             | required | description |
|-------------------|----------|-------------|
| `output_file`     | yes      | Output file name. |
| `linking_options` | no       | A string (= `additional_link_options`) **or** an object: |

`linking_options` as an object:
- `linker` (string, optional) — path to the linker.
- `additional_link_options` (string, optional) — raw linker flags.
- `link_c_standard_library` (bool, optional, defaults to `true`).

> **IMPORTANT — linking against other packages.** To link against another
> package built as `lib`, listing it in `dependencies` is **not enough**.
> You must pass the **full path to the `.a` file** (the static library
> produced by the `lib` task) inside `additional_link_options`. The driver
> does not resolve those paths automatically.

### `strategy: "obj"` — LLVM object files only

Compiles every module of the package to an LLVM object file. No linking
and no archiving is performed — the produced `.o` files are simply left
as build artifacts. Useful when the final link step is performed
externally, or when you only want to type-check / codegen without
finalizing an output binary.

This strategy takes **no extra fields** (`output_file`, `linking_options`,
`archive_options` are all ignored / not applicable).

### `strategy: "lib"` — static library (LLVM → `.a`)
| field             | required | description |
|-------------------|----------|-------------|
| `output_file`     | yes      | Output file name. |
| `archive_options` | no       | A string (= `archiver`) **or** an object with an `archiver` field (path to `ar`). |

## Example — single package, DVM target

```json
{
  "packages": [
    { "id": "app", "name": "app", "path": "src/app" }
  ],
  "tasks": [
    { "package": "app", "strategy": "dvm_exe", "output_file": "app" }
  ]
}
```

## Example — DVM executable depending on a `.dbc`

Build `mathlib` as a DVM library first, then `app` as a DVM executable that
links the resulting `.dbc` and loads a shared library at runtime.

```json
{
  "packages": [
    { "id": "mathlib", "name": "mathlib", "path": "libs/mathlib" },
    {
      "id": "app",
      "name": "app",
      "path": "src/app",
      "dependencies": [ { "id": "mathlib", "alias": "math" } ]
    }
  ],
  "tasks": [
    {
      "package": "mathlib",
      "strategy": "dvm_lib",
      "output_file": "bin/mathlib_dvm.dbc"
    },
    {
      "package": "app",
      "strategy": "dvm_exe",
      "output_file": "bin/app",
      "dvm_linking_options": {
        "shared_libraries": [ "libm.so.6" ],
        "link_libraries": [ "bin/mathlib_dvm.dbc" ]
      }
    }
  ]
}
```

## Example — native executable depending on a `.a`

Build `mathlib` as a static library first, then `app` as an executable
that links the resulting `.a` file.

```json
{
  "packages": [
    {
      "id": "mathlib",
      "name": "mathlib",
      "path": "libs/mathlib",
      "version": "0.1.0"
    },
    {
      "id": "app",
      "name": "app",
      "path": "src/app",
      "dependencies": [
        { "id": "mathlib", "alias": "math" }
      ]
    }
  ],
  "tasks": [
    {
      "package": "mathlib",
      "strategy": "lib",
      "output_file": "libmathlib",
      "archive_options": { "archiver": "/usr/bin/ar" }
    },
    {
      "package": "app",
      "strategy": "native",
      "output_file": "app",
      "linking_options": {
        "linker": "/usr/bin/ld",
        "additional_link_options": "/abs/path/to/build/libmathlib.a",
        "link_c_standard_library": true
      }
    }
  ]
}
```

## Example — `linking_options` in short (string) form

When only raw linker flags are needed:

```json
{
  "package": "app",
  "strategy": "native",
  "output_file": "app",
  "linking_options": "/abs/path/to/libfoo.a /abs/path/to/libbar.a -lpthread"
}
```
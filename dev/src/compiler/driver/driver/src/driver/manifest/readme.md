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
| `name`         | string   | yes      | Package ID (must be unique within the manifest). |
| `path`         | string   | yes      | Path to the package's source root (root module). |
| `version`      | string   | no       | version string. |
| `features`     | string[] | no       | Feature flags |
| `dependencies` | obj[]    | no       | Dependencies — see below. |

Each dependency (`RawDependencyInfo`):

| field   | type   | required | description |
|---------|--------|----------|-------------|
| `name`  | string | yes      | Name of the dependency package. **Must have its own entry in `packages[]`**. |
| `alias` | string | no       | Name used to import the package in code. Defaults to `name`. |

Validation rules (`verify`):
- `packages` must not be empty.
- Package names must be unique.
- Every `dependency.name` must exist in `packages[]`.
- Within a single package: no duplicate `dependency.name` and no duplicate aliases.

## `tasks[]` — compilation tasks (`RawTask`)

Currently a single task type: package compilation. Common fields:

| field      | type   | required | description |
|------------|--------|----------|-------------|
| `package`  | string | yes      | Name of a package from `packages[]` to build. |
| `strategy` | string | yes      | `"dvm"` \| `"native"` \| `"obj"` \| `"lib"`. |

Remaining fields depend on `strategy`:

### `strategy: "dvm"` — DVM bytecode
| field         | required | description |
|---------------|----------|-------------|
| `output_file` | yes      | Output file stem. |

### `strategy: "native"` — native executable (LLVM)
| field             | required | description |
|-------------------|----------|-------------|
| `output_file`     | yes      | Output file stem. |
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
| `output_file`     | yes      | Output file stem. |
| `archive_options` | no       | A string (= `archiver`) **or** an object with an `archiver` field (path to `ar`). |

## Example — single package, DVM target

```json
{
  "packages": [
    { "name": "app", "path": "src/app" }
  ],
  "tasks": [
    { "package": "app", "strategy": "dvm", "output_file": "app" }
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
      "name": "mathlib",
      "path": "libs/mathlib",
      "version": "0.1.0"
    },
    {
      "name": "app",
      "path": "src/app",
      "dependencies": [
        { "name": "mathlib", "alias": "math" }
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

## Common mistakes

- Missing dependency entry in `packages[]` (every dependency must have its own entry, even if it is not the target of any task).
- Two packages sharing a name, or two dependencies of the same package sharing an alias.
- Trying to link a package by declaring it only in `dependencies` — the full path to the `.a` is required in `additional_link_options`.
- Unknown `strategy` value (allowed: `dvm`, `native`, `obj`, `lib`).
- Missing `output_file` for `dvm` / `native` / `lib`.

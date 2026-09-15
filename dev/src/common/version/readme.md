# Version

The version and build information that `duckc`, `VM` and `duck_ls` print for `--version` and
`--version-verbose`.

The module depends on `Base` only, on purpose: `VM` and `duck_ls` must not start pulling in LLVM
just to be able to say which build they are.

## Where the values come from

- **Semantic version** — `project(Duckling VERSION ...)` in `dev/CMakeLists.txt`. `project()` only
  accepts numeric components, so the pre-release suffix sits next to it in
  `DUCKLING_VERSION_PRERELEASE` and the two are composed into `DUCKLING_VERSION_STRING`.
- **Fixed build facts** (build type, host, compiler, licence) — `target_compile_definitions` on the
  `Version` target, evaluated when the build directory is configured.
- **Commit date and dirty flag** — `GenerateCommitInfo.cmake`, run on every build by the
  `VersionCommitInfo` target. It has to run at build time: a date captured at configure time would
  claim the wrong commit for every build after the next commit. The header is written through
  `copy_if_different`, so only `version.cpp` recompiles and only when the values really changed.

`version/commit_info.hpp` is generated into the build directory and may only be included by
`version.cpp`.

## Rendering

`renderShort()` and `renderVerbose()` exist so the three binaries do not each invent their own
layout. A binary-specific fact (duckc's LLVM version, for instance) is passed to `renderVerbose()`
as an `extra` row instead of being added here — this module should not learn about LLVM.

A field with an empty value is left out of the verbose block, which is how the licence lines stay
invisible until a licence is chosen.

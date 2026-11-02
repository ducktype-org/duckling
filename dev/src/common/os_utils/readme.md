# OSUtils

OS syscall and platform API wrappers.

## Platform support

`memory` and `dynamic_library` are implemented for Unix-like systems (Linux,
macOS). On other platforms (e.g. Windows) the module still compiles but the
functions return "not implemented" errors at runtime instead of failing the
build. Real Windows implementations are tracked in
@TODO: #3343 Add Windows CI coverage for os_utils and filepath_utils platform branches.

## What belongs here

- Platform-specific workarounds that affect production code
- In general: any code that branches on `#ifdef` of an OS macro (`__linux__`, `__APPLE__`, `_WIN32`, etc.)

## What does NOT belong here

- CPU architecture detection (x86 vs ARM)
- Type system aliases, those stay in Base
- Test-only constants, keep those in the test file
- URI or path formatting
- Any code that Base relies on



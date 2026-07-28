# OSUtils

OS syscall and platform API wrappers.

## What belongs here

- Platform-specific workarounds that affect production code
- In general: any code that branches on `#ifdef` of an OS macro (`__linux__`, `__APPLE__`, `_WIN32`, etc.)

## What does NOT belong here

- CPU architecture detection (x86 vs ARM)
- Type system aliases, those stay in Base
- Test-only constants, keep those in the test file
- URI or path formatting
- Any code that Base relies on



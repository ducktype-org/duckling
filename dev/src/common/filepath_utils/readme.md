# FilepathUtils

Path-to-URI formatting. Exists as a separate module to avoid a circular dependency:
OSUtils already depends on Filesystem, and Filesystem needed the URI formatter which
contains a Windows drive-letter check, putting it in OSUtils would create a cycle.

## What belongs here

- Path formatting that varies by platform (e.g. `file:///C:` vs `file://`)

## What does NOT belong here

- General path manipulation, that stays in Filesystem
- OS syscall wrappers, those go in OSUtils
- Anything not related to path formatting

## Adding a new file

1. Add the source to `CMakeLists.txt`
2. This module should remain a leaf. Do not add dependencies beyond Base

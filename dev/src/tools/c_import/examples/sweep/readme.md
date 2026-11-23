# Library sweep

`sweep.sh` generates a C package, with the build directory's `duckbind`, for every library in
`libs.txt` that pkg-config finds. Generation builds each package and checks its layouts against
clang's. For the libraries with a program in `smoke/`, it then builds that program against the
package and runs it; each program checks a known result (a hash, a compression round trip, an
SQL query) and exits with a distinct code per check.

```bash
DUCK=path/to/duck ./sweep.sh <duckling-build-dir> <work-dir> [<library>...]
```

The packages, apps and per-library logs are left in `<work-dir>`. The smoke programs run on the
native backend; the DVM does not load `external-libs` yet (#4144).

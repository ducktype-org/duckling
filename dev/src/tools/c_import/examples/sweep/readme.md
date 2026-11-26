# Library sweep

`sweep.sh` generates a C package, with the build directory's `duckbind`, for every library in
`libs.txt` that pkg-config finds. Generation builds each package and checks its layouts against
clang's. For the libraries with a program in `smoke/`, it then builds that program against the
package and runs it; each program checks a known result (a hash, a compression round trip, an
SQL query) and exits with a distinct code per check.

CI does not run the sweep. It is the check of duckbind against real headers, so run it after
changing how headers are read, lowered or emitted. `smoke/zlib.dk` is also run by the integration
test `tools/duckbind` (`zlib/round-trip`).

## Installing the libraries

These install a C compiler, pkg-config and the development packages of the sweep's libraries.
Libraries a distribution does not package are reported as `missing` and skipped.

```bash
# Arch Linux
sudo pacman -S --needed gcc pkgconf zlib curl sqlite libpng openssl freetype2 cairo glfw libuv jansson libxml2 lua ncurses zstd lz4 sdl2-compat libgit2 libsodium gmp libyaml expat raylib sdl3

# Debian 13, Ubuntu 25.04 and later (no raylib; on Ubuntu 24.04 also leave out libsdl3-dev)
sudo apt install build-essential pkg-config zlib1g-dev libcurl4-openssl-dev libsqlite3-dev libpng-dev libssl-dev libfreetype-dev libcairo2-dev libglfw3-dev libuv1-dev libjansson-dev libxml2-dev liblua5.4-dev libncurses-dev libzstd-dev liblz4-dev libsdl2-dev libgit2-dev libsodium-dev libgmp-dev libyaml-dev libexpat1-dev libsdl3-dev

# Fedora (by the pkg-config names in libs.txt)
sudo dnf install gcc pkgconf-pkg-config 'pkgconfig('{zlib,libcurl,sqlite3,libpng16,libcrypto,freetype2,cairo,glfw3,libuv,jansson,libxml-2.0,lua,ncursesw,libzstd,liblz4,sdl2,libgit2,libsodium,gmp,yaml-0.1,expat,raylib,sdl3}')'

# Alpine
doas apk add build-base pkgconf zlib-dev curl-dev sqlite-dev libpng-dev openssl-dev freetype-dev cairo-dev glfw-dev libuv-dev jansson-dev libxml2-dev lua5.4-dev ncurses-dev zstd-dev lz4-dev sdl2-dev libgit2-dev libsodium-dev gmp-dev yaml-dev expat-dev raylib-dev sdl3-dev

# macOS (Homebrew); keg-only formulae are not on pkg-config's path by default
brew install pkgconf zlib curl sqlite libpng openssl@3 freetype cairo glfw libuv jansson libxml2 lua ncurses zstd lz4 sdl2-compat libgit2 libsodium gmp libyaml expat raylib sdl3 && export PKG_CONFIG_PATH="$(for f in zlib curl sqlite openssl@3 libxml2 ncurses expat; do printf '%s:' "$(brew --prefix "$f")/lib/pkgconfig"; done)$PKG_CONFIG_PATH"
```

On Debian and Ubuntu, curl's smoke program does not link yet: duck takes `libcurl.a` before
`libcurl.so`, and the archive's own dependencies are not linked (#4140).

## Running it

```bash
DUCK=path/to/duck ./sweep.sh <duckling-build-dir> <work-dir> [<library>...]
```

The packages, apps and per-library logs are left in `<work-dir>`. The smoke programs run on the
native backend; the DVM does not load `external-libs` yet (#4144).

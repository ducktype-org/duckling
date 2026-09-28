#!/usr/bin/env bash
# Generates SDL3 bindings with `duck translate-c`, then builds and runs a small SDL3 program with
# them on the native backend and on the DVM.
#
# Usage: sdl3_demo.sh <duckling-bin-dir> [--window] [--backend native|dvm|both] [--work <dir>]
#
#   <duckling-bin-dir>  build directory's `bin`, holding duckc, VM and duck_c_import
#   --window            open a real window (bouncing square; Escape or closing it quits) instead
#                       of the headless check
#   --backend           which backend to run on (default: both)
#   --work              where to put the generated package and the program (default: a new
#                       temporary directory)
#
# `duck` is taken from $DUCK, then <duckling-bin-dir>, then PATH. Needs SDL3 visible to pkg-config.
set -euo pipefail

usage() {
	sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
	exit 2
}

[ $# -ge 1 ] || usage
BIN_DIR="$(cd "$1" && pwd)"
shift
WINDOW=0
BACKEND=both
WORK=""
while [ $# -gt 0 ]; do
	case "$1" in
		--window) WINDOW=1 ;;
		--backend) BACKEND="$2"; shift ;;
		--work) WORK="$2"; shift ;;
		-h|--help) usage ;;
		*) echo "unknown argument: $1" >&2; usage ;;
	esac
	shift
done

export PATH="$BIN_DIR:$PATH"
for tool in duckc VM duck_c_import; do
	if ! command -v "$tool" >/dev/null; then
		echo "error: \`$tool\` is not in $BIN_DIR (duck_c_import is only built when libclang is found)" >&2
		exit 1
	fi
done
DUCK="${DUCK:-$(command -v duck || true)}"
if [ -z "$DUCK" ]; then
	echo "error: \`duck\` not found; set DUCK or build it with \`cargo build --release\` in src/duck" >&2
	exit 1
fi
if ! pkg-config --exists sdl3; then
	echo "error: pkg-config cannot find sdl3" >&2
	exit 1
fi

if [ -z "$WORK" ]; then WORK="$(mktemp -d -t duck-sdl3-XXXXXX)"; fi
mkdir -p "$WORK"
WORK="$(cd "$WORK" && pwd)"
cd "$WORK"
echo "working in $WORK (SDL3 $(pkg-config --modversion sdl3))"

# 1. The bindings: a package local to this machine.
rm -rf sdl3 demo
"$DUCK" translate-c new sdl3 --pkg-config sdl3 --header SDL3/SDL.h --include 'SDL_*' --include 'SDLK_*'

# 2. A program using them.
"$DUCK" init demo >/dev/null
"$DUCK" -C demo add --local ../sdl3 sdl3 >/dev/null
printf 'profiles:\n  dvm:\n    dvm-bytecode: true\n' >> demo/quackconfig.yaml

if [ "$WINDOW" = 1 ]; then
cat > demo/src/src.dk <<'EOF'
import core.builtins.*;
import sdl3.*;

# A square bouncing around a window, until it is closed or Escape is pressed (at most 30 seconds).
fun main() -> i64 = {
    if (SDL_Init(SDL_INIT_VIDEO) == false) { return 1i64; }
    let title = ptr_from_slice:{char}("duckling + SDL3\0") as cptr char;
    let window = SDL_CreateWindow(title, 640i32, 480i32, 0u64);
    if (SDL_GetWindowID(window) == 0u32) { return 2i64; }
    let renderer = SDL_CreateRenderer(window, ptr_from_slice:{char}("software\0") as cptr char);

    var square: SDL_FRect;
    square.x = 40.0f32;
    square.y = 30.0f32;
    square.w = 80.0f32;
    square.h = 80.0f32;
    var dx: f32 = 4.0f32;
    var dy: f32 = 3.0f32;

    var event: SDL_Event;
    let ev = &event as cptr SDL_Event;
    var frames: i64 = 0i64;
    var running = true;
    while (running) {
        while (SDL_PollEvent(ev)) {
            let kind = SDL_Event_as_type(ev)[0];
            if (kind == SDL_EVENT_QUIT) { running = false; }
            if (kind == SDL_EVENT_KEY_DOWN) {
                if (SDL_Event_as_key(ev)[0].key == SDLK_ESCAPE) { running = false; }
            }
        }
        if (running) {
            square.x = square.x + dx;
            square.y = square.y + dy;
            if (square.x < 0.0f32) { dx = 0.0f32 - dx; }
            if (square.x > 560.0f32) { dx = 0.0f32 - dx; }
            if (square.y < 0.0f32) { dy = 0.0f32 - dy; }
            if (square.y > 400.0f32) { dy = 0.0f32 - dy; }

            let background = SDL_SetRenderDrawColor(renderer, 20u8, 20u8, 40u8, 255u8);
            let cleared = SDL_RenderClear(renderer);
            let yellow = SDL_SetRenderDrawColor(renderer, 255u8, 200u8, 0u8, 255u8);
            let filled = SDL_RenderFillRect(renderer, &square as cptr SDL_FRect);
            let presented = SDL_RenderPresent(renderer);
            SDL_Delay(16u32);
            frames = frames + 1i64;
            if (frames > 1800i64) { running = false; }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0i64;
}
EOF
else
cat > demo/src/src.dk <<'EOF'
import core.builtins.*;
import sdl3.*;

# Runs the event loop for a few frames drawing a yellow square, reads one of its pixels back, then
# quits through a pushed `SDL_EVENT_QUIT`. The exit code says which step failed.
fun main() -> i64 = {
    if (SDL_Init(SDL_INIT_VIDEO) == false) { return 1i64; }

    let title = ptr_from_slice:{char}("duckling + SDL3\0") as cptr char;
    let window = SDL_CreateWindow(title, 320i32, 240i32, 0u64);
    if (SDL_GetWindowID(window) == 0u32) { return 2i64; }
    let renderer = SDL_CreateRenderer(window, ptr_from_slice:{char}("software\0") as cptr char);

    var square: SDL_FRect;
    square.x = 100.0f32;
    square.y = 60.0f32;
    square.w = 120.0f32;
    square.h = 120.0f32;

    var event: SDL_Event;
    let ev = &event as cptr SDL_Event;
    var frames: i64 = 0i64;
    var running = true;
    var saw_quit = false;
    while (running) {
        while (SDL_PollEvent(ev)) {
            if (SDL_Event_as_type(ev)[0] == SDL_EVENT_QUIT) {
                running = false;
                saw_quit = true;
            }
        }

        if (running) {
            let background = SDL_SetRenderDrawColor(renderer, 20u8, 20u8, 40u8, 255u8);
            let cleared = SDL_RenderClear(renderer);
            let yellow = SDL_SetRenderDrawColor(renderer, 255u8, 200u8, 0u8, 255u8);
            if (SDL_RenderFillRect(renderer, &square as cptr SDL_FRect) == false) { return 3i64; }

            frames = frames + 1i64;
            if (frames == 3i64) {
                var area: SDL_Rect;
                area.x = 150i32;
                area.y = 100i32;
                area.w = 1i32;
                area.h = 1i32;
                let surface = SDL_RenderReadPixels(renderer, &area as cptr SDL_Rect);
                var r: u8 = 0u8;
                var g: u8 = 0u8;
                var b: u8 = 0u8;
                var a: u8 = 0u8;
                if (SDL_ReadSurfacePixel(surface, 0i32, 0i32, &r as cptr u8, &g as cptr u8, &b as cptr u8, &a as cptr u8) == false) { return 4i64; }
                if (r != 255u8) { return 5i64; }
                if (g != 200u8) { return 6i64; }
                if (b != 0u8) { return 7i64; }
                SDL_DestroySurface(surface);

                var quit: SDL_Event;
                let q = &quit as cptr SDL_Event;
                let quit_event = SDL_Event_as_quit(q);
                quit_event[0].type_ = SDL_EVENT_QUIT;
                if (SDL_PushEvent(q) == false) { return 8i64; }
            }
            let presented = SDL_RenderPresent(renderer);
            if (frames > 100i64) { return 9i64; }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (saw_quit == false) { return 10i64; }
    if (frames != 3i64) { return 11i64; }
    return 0i64;
}
EOF
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-offscreen}"
fi

# 3. Run it.
status=0
run() {
	local name="$1" profile="$2"
	echo "== $name"
	if "$DUCK" -C demo run --profile="$profile"; then
		echo "== $name: ok"
	else
		local code=$?
		echo "== $name: failed with exit code $code" >&2
		status=1
	fi
}
case "$BACKEND" in
	native) run native dev ;;
	dvm) run DVM dvm ;;
	both) run native dev; run DVM dvm ;;
	*) echo "unknown backend: $BACKEND" >&2; exit 2 ;;
esac
exit "$status"

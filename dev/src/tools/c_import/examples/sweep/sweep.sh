#!/usr/bin/env bash
# Generates a C package for every library in libs.txt that pkg-config finds, checks its layouts,
# and runs smoke/<name>.dk against it when there is one.
#
# Usage: sweep.sh <duckling-build-dir> <work-dir> [<name>...]
#
# `duckbind` comes from the build directory, `duck` from $DUCK or PATH. Prints one line per
# library and a summary table.
set -uo pipefail

[ $# -ge 2 ] || { sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
SWEEP_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="$(cd "$1" && pwd)"
if [ -x "$BIN_DIR/bin/duckc" ]; then BIN_DIR="$BIN_DIR/bin"; fi
WORK="$2"
shift 2
ONLY=" $* "
export PATH="$BIN_DIR:$PATH"
DUCK="${DUCK:-$(command -v duck)}"
mkdir -p "$WORK"
WORK="$(cd "$WORK" && pwd)"

trim() { local s="$1"; s="${s#"${s%%[![:space:]]*}"}"; echo "${s%"${s##*[![:space:]]}"}"; }

printf '%-9s %-12s %-10s %8s %9s %9s %7s  %s\n' library version generate classes functions constants skipped smoke
status=0
while IFS='|' read -r name pkg headers includes; do
	name="$(trim "$name")"
	case "$name" in ''|'#'*) continue ;; esac
	[ "$ONLY" = "  " ] || [[ "$ONLY" == *" $name "* ]] || continue
	pkg="$(trim "$pkg")"; headers="$(trim "$headers")"; includes="$(trim "$includes")"
	if ! pkg-config --exists "$pkg"; then
		printf '%-9s %-12s %-10s\n' "$name" - missing
		continue
	fi
	version="$(pkg-config --modversion "$pkg")"
	dir="$WORK/$name"
	rm -rf "$dir" "$WORK/$name-app"
	args=(new "$dir" --pkg-config "$pkg" --header "${headers// /,}" --duck "$DUCK")
	if [ "$includes" != "-" ]; then args+=(--include "${includes// /,}"); fi
	log="$WORK/$name.log"
	if ! duckbind "${args[@]}" > "$log" 2>&1; then
		printf '%-9s %-12s %-10s  see %s\n' "$name" "$version" FAILED "$log"
		status=1
		continue
	fi
	counts="$(sed -n 's/.*generated `[^`]*`: \([0-9]*\) classes, \([0-9]*\) functions, \([0-9]*\) constants.*/\1 \2 \3/p' "$log")"
	skipped="$(sed -n 's/.*skipped \([0-9]*\) declarations.*/\1/p' "$log")"
	smoke=-
	if [ -f "$SWEEP_DIR/smoke/$name.dk" ]; then
		app="$WORK/$name-app"
		if "$DUCK" init "$app" > /dev/null 2>&1 \
			&& "$DUCK" -C "$app" add --local "$dir" "$name" > /dev/null 2>&1 \
			&& cp "$SWEEP_DIR/smoke/$name.dk" "$app/src/src.dk" \
			&& "$DUCK" -C "$app" run >> "$log" 2>&1; then
			smoke=ok
		else
			smoke="FAILED($?)"
			status=1
		fi
	fi
	read -r classes functions constants <<< "$counts"
	printf '%-9s %-12s %-10s %8s %9s %9s %7s  %s\n' "$name" "$version" ok \
		"${classes:--}" "${functions:--}" "${constants:--}" "${skipped:-0}" "$smoke"
done < "$SWEEP_DIR/libs.txt"
exit "$status"

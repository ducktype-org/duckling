#!/usr/bin/env bash
# Usage: apt-get.sh            -> apt-get update
#        apt-get.sh PKG...     -> apt-get install PKG...
#
# apt runs as root, so when a job is cancelled the runner cannot kill it. On a self-hosted runner
# that outlives the job, a hung apt then holds the apt lock and fails every later job. Hence every
# apt call here is time-limited, retried while the lock is busy, and leftovers are killed.
set -euo pipefail

readonly CALL_LIMIT=15m
readonly STALE_AFTER_SECONDS=1800
readonly LOCK_WAIT_SECONDS=1200

sudo tee /etc/apt/apt.conf.d/99-ci-timeouts >/dev/null <<'CONF'
Acquire::Retries "3";
Acquire::http::Timeout "60";
Acquire::https::Timeout "60";
CONF

# No apt call started by this script lives past CALL_LIMIT, so older apt processes are leftovers.
kill_stale_apt() {
    command -v ps >/dev/null || return 0
    local stale
    stale=$(ps -eo pid=,etimes=,comm=,args= |
        awk -v limit="$STALE_AFTER_SECONDS" \
            '$2 > limit && ($3 ~ /^(apt|apt-get|dpkg)$/ || $4 ~ /^\/usr\/lib\/apt\/methods\//) { print $1 }')
    [[ -n $stale ]] || return 0
    echo "::warning::Killing apt processes left behind by an earlier job: $(echo $stale)"
    # shellcheck disable=SC2086
    sudo kill -9 $stale || true
    sudo env DEBIAN_FRONTEND=noninteractive timeout --kill-after=30s "$CALL_LIMIT" \
        dpkg --configure -a || echo "::warning::dpkg --configure -a failed, apt-get will retry"
}

log=$(mktemp)
trap 'rm -f "$log"' EXIT

# DPkg::Lock::Timeout does not cover the lists lock taken by `apt-get update`, so wait here.
run() {
    local deadline=$((SECONDS + LOCK_WAIT_SECONDS))
    while true; do
        kill_stale_apt
        local status=0
        # A pipeline rather than `2> >(tee ...)`, so tee has finished writing $log before the grep.
        {
            sudo env DEBIAN_FRONTEND=noninteractive timeout --kill-after=30s "$CALL_LIMIT" \
                apt-get "$@" 2>&1 >&3 3>&- | tee "$log" >&2 3>&-
        } 3>&1 || status=$?
        if ((status == 0)) || ! grep -q 'Could not get lock' "$log" || ((SECONDS >= deadline)); then
            return "$status"
        fi
        echo "apt lock is busy, retrying in 10s"
        sleep 10
    done
}

if [[ $# -eq 0 ]]; then
    run update
else
    run -y install "$@"
fi

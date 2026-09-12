#!/usr/bin/env python3
"""
Deletes the cache entries a branch has superseded, keeping the newest of each.

Every run of `tests.yml` saves cache entries under keys ending in a fresh
timestamp, so without pruning a branch accumulates one set per run until they
start evicting each other -- and the repository-wide limit is shared with every
other branch.

Entries are grouped by the key they would share if the timestamp were removed;
the most recently accessed member of each group is kept and the rest deleted.
Grouping, rather than "delete everything matching this prefix", is what lets
this run once after the whole build matrix instead of once per matrix entry:
deleting by prefix after the builds have uploaded would delete what they just
saved.

This talks to the REST API directly, so it needs neither `gh` nor the
`actions/gh-actions-cache` extension -- the mac runners have no `gh` and their
accounts cannot install one.

The token comes from the environment rather than a flag: a command line is
visible to anything that can list processes, and these runners are shared.
"""

import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.request
from typing import Any

API_ROOT = "https://api.github.com"

# The suffix "Prepare cache timestamp" in tests.yml appends to every key:
# cmake's "%Y-%m-%d-%H::%M::%S". It is the only part of a key that differs
# between two runs of the same configuration on the same branch.
TIMESTAMP_SUFFIX = re.compile(r"-\d{4}-\d{2}-\d{2}-\d{2}::\d{2}::\d{2}$")

# Stop rather than page forever if the API keeps returning full pages.
MAX_PAGES = 20
PER_PAGE = 100


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, help="owner/name")
    parser.add_argument("--branch", required=True, help="branch whose entries to prune")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="report what would be deleted without deleting it",
    )
    return parser.parse_args()


def api(method: str, url: str, token: str) -> Any:
    request = urllib.request.Request(
        url=url,
        method=method,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    with urllib.request.urlopen(request) as response:
        body = response.read()
    return json.loads(body) if body else None


def list_caches(repo: str, token: str) -> list[dict[str, Any]]:
    """
    Every cache entry in the repository, most recently accessed first.

    Collected in full before anything is deleted: deleting while paging would
    shift the later pages and skip entries.
    """
    caches: list[dict[str, Any]] = []
    for page in range(1, MAX_PAGES + 1):
        url = (
            f"{API_ROOT}/repos/{repo}/actions/caches"
            f"?per_page={PER_PAGE}&page={page}"
            f"&sort=last_accessed_at&direction=desc"
        )
        batch = api("GET", url, token).get("actions_caches", [])
        caches += batch
        if len(batch) < PER_PAGE:
            break
    return caches


def group_of(key: str) -> str:
    """The key with its per-run timestamp removed."""
    return TIMESTAMP_SUFFIX.sub("", key)


def plan(
    caches: list[dict[str, Any]], branch: str
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    """
    Split the entries into (keep, delete). Pure, so it can be reasoned about
    and tested without touching the API.

    `caches` must already be ordered newest-accessed first; the first entry seen
    for a group is the one kept.

    An entry belongs to this branch when its group ENDS WITH the branch name --
    not merely contains it. Every key is built as
    `<os>-<prefix>-<build-type>-<branch>-<timestamp>` or
    `<os>-sccache-<branch>-<timestamp>`, so the branch is always last once the
    timestamp is gone. A substring test would let branch `foo` claim the entries
    of `foo-bar`, and delete them.
    """
    keep: list[dict[str, Any]] = []
    delete: list[dict[str, Any]] = []
    seen: set[str] = set()

    for cache in caches:
        group = group_of(cache["key"])
        if not group.endswith(f"-{branch}"):
            continue
        if group in seen:
            delete.append(cache)
        else:
            seen.add(group)
            keep.append(cache)

    return keep, delete


def main() -> int:
    args = parse_args()

    token = os.environ.get("CACHE_DELETE_TOKEN", "")
    if not token:
        print("CACHE_DELETE_TOKEN is empty; nothing can be pruned.", file=sys.stderr)
        return 1

    caches = list_caches(args.repo, token)
    keep, delete = plan(caches, args.branch)
    print(f"{len(caches)} cache entries in {args.repo}, branch {args.branch!r}:")
    for cache in keep:
        print(f"  keep   {cache['key']}")

    failed = 0
    for cache in delete:
        if args.dry_run:
            print(f"  WOULD DELETE {cache['key']}")
            continue
        try:
            api(
                "DELETE",
                f"{API_ROOT}/repos/{args.repo}/actions/caches/{cache['id']}",
                token,
            )
            print(f"  delete {cache['key']}")
        except urllib.error.URLError as error:
            # Losing a cache entry is not worth failing a build over, and a
            # concurrent run may well have deleted this one already.
            print(f"  FAILED to delete {cache['key']}: {error}", file=sys.stderr)
            failed += 1

    print(f"kept {len(keep)}, deleted {len(delete) - failed}, failed {failed}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

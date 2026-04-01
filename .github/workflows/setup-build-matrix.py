#!/usr/bin/env python3
import argparse
import json
import urllib.request
from typing import Any


FULL_MATRIX: dict[str, list[Any]] = {
    "build-type": ["Dev", "DevOpt"],
    "compiler": [
        {
            "name": "gcc",
            "cxx": "g++-14",
            "cc": "gcc-14",
            "gcov": "gcov-14",
            "linker": "mold",
            "cache-prefix": "gcc-build",
        },
        {
            "name": "clang",
            "cxx": "clang++-19",
            "cc": "clang-19",
            "linker": "mold",
            "cache-prefix": "clang-build",
        },
    ],
}

PR_MATRIX: dict[str, list[Any]] = {
    "build-type": ["Dev"],
    "compiler": [
        {
            "name": "gcc",
            "cxx": "g++-14",
            "cc": "gcc-14",
            "gcov": "gcov-14",
            "linker": "mold",
            "cache-prefix": "gcc-build",
        }
    ],
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Select tests workflow build matrix.")
    parser.add_argument("--event-name", required=True)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--branch", required=True)
    parser.add_argument("--pr-number", type=int, required=True)
    parser.add_argument("--github-token", required=True)
    return parser.parse_args()


def has_approval(repo: str, pr_number: int, github_token: str) -> bool:
    req = urllib.request.Request(
        url=f"https://api.github.com/repos/{repo}/pulls/{pr_number}/reviews?per_page=100",
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {github_token}",
        },
    )
    with urllib.request.urlopen(req) as response:
        reviews = json.loads(response.read())
    return any(review.get("state") == "APPROVED" for review in reviews)


def has_label(repo: str, pr_number: int, github_token: str, label_name: str) -> bool:
    req = urllib.request.Request(
        url=f"https://api.github.com/repos/{repo}/issues/{pr_number}/labels?per_page=100",
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {github_token}",
        },
    )
    with urllib.request.urlopen(req) as response:
        labels = json.loads(response.read())
    return any(label.get("name") == label_name for label in labels)


def main():
    args = parse_args()

    matrix = FULL_MATRIX
    is_non_main_branch = args.branch not in ["main", "dev"]
    is_pull_request = args.event_name == "pull_request"
    has_auth = bool(args.github_token)

    if is_non_main_branch and is_pull_request:
        if not has_auth:
            matrix = PR_MATRIX
        elif not has_label(args.repo, args.pr_number, args.github_token, "Run All Workflows"):
            if not has_approval(args.repo, args.pr_number, args.github_token):
                matrix = PR_MATRIX

    print(json.dumps(matrix))


if __name__ == "__main__":
    main()

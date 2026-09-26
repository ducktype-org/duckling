#!/usr/bin/env python3
import argparse
import json
import urllib.request
from typing import Any


LINUX_GCC: dict[str, Any] = {
    "name": "gcc",
    "cxx": "g++-14",
    "cc": "gcc-14",
    "gcov": "gcov-14",
    "linker": "mold",
    "cache-prefix": "gcc-build",
    "platform": "linux",
}

LINUX_CLANG: dict[str, Any] = {
    "name": "clang",
    "cxx": "clang++-19",
    "cc": "clang-19",
    "linker": "mold",
    "cache-prefix": "clang-build",
    "platform": "linux",
}

MACOS_CLANG: dict[str, Any] = {
    "name": "clang-23-macos",
    "cxx": "/opt/homebrew/opt/llvm@23/bin/clang++",
    "cc": "/opt/homebrew/opt/llvm@23/bin/clang",
    "linker": "lld",
    "cache-prefix": "clang-build",
    "platform": "macos",
}

FULL_MATRIX: dict[str, list[Any]] = {
    "include": [
        {"build-type": "Dev", "compiler": LINUX_GCC},
        {"build-type": "Dev", "compiler": MACOS_CLANG},
        {"build-type": "DevOpt", "compiler": LINUX_GCC},
        {"build-type": "DevOpt", "compiler": LINUX_CLANG},
    ],
}

PR_MATRIX: dict[str, list[Any]] = {
    "include": [
        {"build-type": "Dev", "compiler": LINUX_GCC},
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


def should_use_full_matrix(args: argparse.Namespace) -> bool:
    if args.branch in ["main", "dev"]:
        return True

    if args.event_name != "pull_request":
        return True

    labeled = args.github_token and has_label(
        args.repo, args.pr_number, args.github_token, "Run All Workflows"
    )
    approved = args.github_token and has_approval(args.repo, args.pr_number, args.github_token)
    return bool(labeled or approved)


def main():
    args = parse_args()

    matrix = FULL_MATRIX if should_use_full_matrix(args) else PR_MATRIX

    print(json.dumps(matrix))


if __name__ == "__main__":
    main()

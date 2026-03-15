from hashlib import sha256
from pathlib import Path


def collect_artifact_snapshot(build_dir: Path) -> dict[str, str]:
    if not build_dir.exists():
        return {}

    files = sorted(
        file for file in build_dir.rglob("*") if file.is_file() and file.suffix in {".o", ".dbc"}
    )

    snapshot: dict[str, str] = {}
    for file in files:
        rel = file.relative_to(build_dir).as_posix()
        snapshot[rel] = sha256(file.read_bytes()).hexdigest()
    return snapshot


def diff_snapshots(single: dict[str, str], concurrent: dict[str, str]) -> list[str]:
    diffs: list[str] = []

    single_paths = set(single.keys())
    concurrent_paths = set(concurrent.keys())

    missing_in_concurrent = sorted(single_paths - concurrent_paths)
    extra_in_concurrent = sorted(concurrent_paths - single_paths)

    if missing_in_concurrent:
        diffs.append("Missing in concurrent build: " + ", ".join(missing_in_concurrent[:10]))
    if extra_in_concurrent:
        diffs.append("Extra in concurrent build: " + ", ".join(extra_in_concurrent[:10]))

    changed = sorted(path for path in (single_paths & concurrent_paths) if single[path] != concurrent[path])
    if changed:
        diffs.append("Content differs: " + ", ".join(changed[:10]))

    return diffs

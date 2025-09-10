### quackpack.core

Core domain logic for QuackPack.

## `signals.py`
Robust signal handling utilities and contexts.

## `package_loader.py`
Finds manifests (`quackconfig.yml`), venv config, and local storage; builds `Package`.

## `constants.py`
Core configuration constants.

### Subpackages
- `fetcher/`: HTTP/Git clients, caching and helpers to fetch metadata and blobs from registries.
  - `client/`: `curl_http_client.py`, `ducknest_client.py`, `git_client.py`.
  - `cache/`: `metadata_cache.py`, `sqlite_database.py`.
  - `util/`: HTTP helpers, compression, progress reporting, errors.
- `solver/`: dependency solving and build rules construction.
  - `gathering/`: collect info from manifests and registry.
  - `solving/`: model, engine and build rules.
  - `cleaning/`: post-solve cleanup helpers.
  - `types/`: typed IDs, packages and enums used by solver.
- `storage/`: file layout, paths, lock handling, and high-level storage orchestration.
- `types/`: manifest models (schemas and parsed forms) and package representation.

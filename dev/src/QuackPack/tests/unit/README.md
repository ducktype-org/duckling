### Unit tests

Brief overview of what each test covers:

## `test_env.py`
Behavior of `quackpack.util.env.Env` (case sensitivity on different platforms).

## `test_identifiers.py`
Identifier validation in `util.duckling_compatibility.is_valid_identifier`.

## `test_levenshtein.py`
Levenshtein distance implementation in `driver.cli.levenshtein`.

## `test_package_loader.py`
Discovery of manifests and packages in `core.package_loader.PackageLoader`.

## `test_package.py`
Basic manifest discovery and `Package` summary shape.

## `version/test_version.py`
Parsing, comparison and bumping logic in `util.types.version.Version`.

## `yaml/test_deserialisation.py`
Strict YAML loader validation flows and error formatting.

## `yaml/test_validation.py`
Lower-level YAML loader positioning and error cases.

## `manifest/test_manifest.py`
Manifest parsing into summary, dependency sources, features, and validation errors.

## `fetcher/test_http_client.py`
Async HTTP client (pycurl-based) behaviors, error handling and timeouts.

## `fetcher/test_utils.py`
HTTP helpers (headers parsing, request/response wrappers, error types).

## `fetcher/test_ducknest_client.py`
Ducknest API client calls against a test FastAPI server.

## `fetcher/test_cache.py`
SQLite-backed metadata cache and context manager behavior.

## `fetcher/test_git_client.py`
Git cloning with branch/tag/rev and error paths.

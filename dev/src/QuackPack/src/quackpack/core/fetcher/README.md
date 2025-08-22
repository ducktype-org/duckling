### quackpack.core.fetcher

Fetching package metadata and blobs from registries and VCS.

## `fetcher.py`
High-level context and operations to get metadata and blobs.

## `ducknest_endpoints.py`
Endpoints used to talk to the Ducknest registry service.

## `api_types.py`
Pydantic models for registry payloads.

### Submodules
- `client/`: concrete clients
  - `curl_http_client.py`: async HTTP client based on pycurl with concurrency.
  - `ducknest_client.py`: thin client for Ducknest HTTP API.
  - `git_client.py`: operations for cloning and checking out git sources.
- `cache/`: local metadata caching and SQLite storage.
- `util/`: HTTP request/response, headers, errors, and progress helpers.

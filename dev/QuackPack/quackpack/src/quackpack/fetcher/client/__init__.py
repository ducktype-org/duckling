"""
Top-level module for HTTP and Git client utilities.
----
Exports:
- `CurlError`: Exception for errors encountered by the `CurlHTTPClient`.
- `CurlHTTPClient`: An asynchronous HTTP client using libcurl.
- `CurlInfo`: A named tuple containing information about a cURL request.
- `DucknestClient`: A client for interacting with Ducknest instances.
- `GitClient`: A client for interacting with Git repositories.
"""

from .curl_http_client import CurlError, CurlHTTPClient, CurlInfo
from .ducknest_client import DucknestClient
from .git_client import GitClient

__all__ = ["CurlError", "CurlHTTPClient", "CurlInfo", "DucknestClient", "GitClient"]

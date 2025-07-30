"""
Top-level module providing interfaces for fetching package metadata, blobs, and Git sources.

This module defines the main entry points for interacting with remote Ducknest instances.
It provides classes to handle caching, downloading, publishing, and searching of packages,
as well as cloning Git repositories containing package sources.
----
Exports:
- `Fetcher`: Core class responsible for package fetching logic.
- `FetcherContext`: Context manager to safely initialize and manage a `Fetcher` instance.
"""

from .fetcher import Fetcher, FetcherContext

__all__ = ["Fetcher", "FetcherContext"]

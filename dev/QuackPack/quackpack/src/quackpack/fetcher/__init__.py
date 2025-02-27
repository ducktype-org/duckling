"""
Top-level module providing classes and utilites for fetching remote packages.
----
Exports:
- `Fetcher`: A class for managing HTTP and Git clients, and caching metadata.
"""

from .fetcher import Fetcher

__all__ = ["Fetcher"]

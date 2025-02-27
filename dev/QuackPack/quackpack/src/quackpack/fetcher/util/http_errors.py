"""
Module for handling HTTP-related errors.
----
Classes:
- `HTTPInputError`: Exception for malformed HTTP requests or responses.
- `HTTPOutputError`: Exception for errors in HTTP output.
- `HTTPError`: Exception for unsuccessful HTTP requests, including status code and response details.
----
References:
  [1] https://github.com/tornadoweb/tornado/blob/master/tornado/httpclient.py
  [2] https://github.com/tornadoweb/tornado/blob/master/tornado/httputil.py
"""

from __future__ import annotations

from quackpack.fetcher.util import HTTPResponse


class HTTPInputError(Exception):
    """
    Exception for malformed HTTP requests or responses from remote sources.
    """


class HTTPOutputError(Exception):
    """
    Exception for errors in HTTP output.
    """


class HTTPError(Exception):
    """
    Exception for unsuccessful HTTP requests.
    ----
    Args:
    - `code`: HTTP error integer code. Code 599 indicates no HTTP response was received (e.g., timeout).
    - `response`: `HTTPResponse` object, if available.
    """

    def __init__(self, code: int, message: str | None = None, response: HTTPResponse | None = None) -> None:
        self.code = code
        # NOTE: Original comment.
        self.message = message  # or httputil.responses.get(code, "Unknown")
        self.response = response
        super().__init__(code, message, response)

    def __str__(self) -> str:
        return f"HTTP {self.code}: {self.message}"

    # NOTE: Original comment.
    # There is a cyclic reference between self and self.response,
    # which breaks the default __repr__ implementation.
    # (especially on pypy, which doesn't have the same recursion
    # detection as cpython).
    __repr__ = __str__

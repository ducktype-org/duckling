"""
Top-level module for HTTP-related utilities.
----
Exports:
- `HTTPError`, `HTTPInputError`, `HTTPOutputError`: HTTP-related exceptions.
- `HTTPHeaders`: Class for handling HTTP headers.
- `HTTPRequest`, `HTTPRequestProxy`: Classes for representing HTTP requests.
- `HTTPResponse`: Class for representing HTTP responses.
- `ResponseStartLine`: Class for representing HTTP response start lines.
- `parse_http1_response_start_line`: Function for parsing HTTP 1.x response start lines.
"""

from .errors import FailedRequestError, MissingEventLoopError, UninitializedClientError
from .http_errors import HTTPError, HTTPInputError, HTTPOutputError
from .http_headers import HTTPHeaders, ResponseStartLine, parse_http1_response_start_line
from .http_request import HTTPRequest as HTTPRequest, HTTPRequestProxy
from .http_response import HTTPResponse as HTTPResponse

__all__ = [
    "FailedRequestError",
    "HTTPError",
    "HTTPHeaders",
    "HTTPInputError",
    "HTTPOutputError",
    "HTTPRequest",
    "HTTPRequestProxy",
    "HTTPResponse",
    "MissingEventLoopError",
    "ResponseStartLine",
    "UninitializedClientError",
    "parse_http1_response_start_line",
]

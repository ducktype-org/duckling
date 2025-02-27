"""
Module for representing and handling HTTP responses.
----
Classes:
- `HTTPResponse`: Represents an HTTP response, including status code, headers, and body.
----
References:
  [1] https://github.com/tornadoweb/tornado/blob/master/tornado/httpclient.py
"""

from __future__ import annotations

from io import BytesIO

from quackpack.fetcher.util import HTTPError, HTTPHeaders, HTTPRequest, HTTPRequestProxy


class HTTPResponse:
    """
    Represents an HTTP response.
    ----
    Args:
    - `request`: The `HTTPRequest` object associated with this response.
    - `code`: The HTTP status code of the response.
    - `headers`: Optional `HTTPHeaders` object containing response headers.
    - `buffer`: Optional `BytesIO` object containing the response body.
    - `effective_url`: Optional string representing the final URL after redirects.
    - `error`: Optional exception object if an error occurred during the request.
    - `request_time`: Optional float representing the time taken for the request.
    - `time_info`: Optional dictionary containing timing information.
    - `reason`: Optional string representing the HTTP reason phrase.
    - `start_time`: Optional float representing the start time of the request.
    """

    def __init__(
        self,
        request: HTTPRequest,
        code: int,
        *,
        headers: HTTPHeaders | None = None,
        buffer: BytesIO | None = None,
        effective_url: str | None = None,
        error: BaseException | None = None,
        request_time: float | None = None,
        time_info: dict[str, float] | None = None,
        reason: str | None = None,
        start_time: float | None = None,
    ) -> None:
        if isinstance(request, HTTPRequestProxy):
            self.request = request.request
        else:
            self.request = request
        self.code = code
        # NOTE: Original comment.
        self.reason = reason  # or responses.get(code, "Unknown")
        if headers is not None:
            self.raw_headers = headers
        else:
            self.raw_headers = HTTPHeaders()

        self.buffer = buffer
        self._body: bytes | None = None
        if effective_url is None:
            self.effective_url = request.url
        else:
            self.effective_url = effective_url
        self._error_is_response_code = False
        if error is None:
            if self.code < 200 or self.code >= 300:
                self._error_is_response_code = True
                self.error = HTTPError(self.code, message=self.reason, response=self)
            else:
                self.error = None
        else:
            self.error = error
        self.start_time = start_time
        self.request_time = request_time
        self.time_info = time_info or {}

    @property
    def body(self) -> bytes:
        """
        Get the response body as bytes.
        ----
        Returns:
        - `bytes`: The response body as bytes.
        """

        if self.buffer is None:
            return b""
        elif self._body is None:
            self._body = self.buffer.getvalue()

        return self._body

    def rethrow(self) -> None:
        """
        If there was an error on the request, raise an `HTTPError`.
        ----
        Raises:
        - `HTTPError`: If an error occurred during the request.
        """

        if self.error:
            raise self.error

    def __repr__(self) -> str:
        args = ",".join(f"{k}={v!r}" for k, v in sorted(self.__dict__.items()))
        return f"{self.__class__.__name__}({args})"

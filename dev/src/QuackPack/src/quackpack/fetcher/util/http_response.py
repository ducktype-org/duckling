"""
Module for representing and handling HTTP responses.
----
Classes:
- `HTTPResponse`: Represents an HTTP response, including status code, headers, and body.
----
References:
  [1] https://github.com/tornadoweb/tornado/blob/master/tornado/httpclient.py
"""

from io import BytesIO
from typing import override

from quackpack.fetcher.util.http_errors import HTTPError
from quackpack.fetcher.util.http_headers import HTTPHeaders
from quackpack.fetcher.util.http_request import HTTPRequest, HTTPRequestProxy


class HTTPResponse:
    """
    Represents an HTTP response.

    :param request: The HTTP request associated with this response.
    :type request: quackpack.fetcher.util.HTTPRequest | quackpack.fetcher.util.HTTPRequestProxy
    :param code: The HTTP status code of the response.
    :type code: int
    :param headers: Response headers.
    :type headers: quackpack.fetcher.util.HTTPHeaders | None
    :param buffer: Buffer containing the response body.
    :type buffer: io.BytesIO | None
    :param effective_url: Final URL after redirects.
    :type effective_url: str | None
    :param error: Exception if an error occurred during the request.
    :type error: BaseException | None
    :param request_time: Time taken for the request in seconds.
    :type request_time: float | None
    :param time_info: Timing information dictionary.
    :type time_info: dict[str, float] | None
    :param reason: HTTP reason phrase.
    :type reason: str | None
    :param start_time: Optional start time of the request.
    :type start_time: float | None
    """

    def __init__(
        self,
        request: HTTPRequest | HTTPRequestProxy,
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
                self.error = HTTPError(self.code, message=self.reason)
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

        :return: The response body.
        :rtype: bytes
        """

        if self.buffer is None:
            return b""
        elif self._body is None:
            self._body = self.buffer.getvalue()

        return self._body

    def rethrow(self) -> None:
        """
        Raise an HTTPError if an error occurred during the request.

        :raises quackpack.fetcher.util.HTTPError: If the response contains an error.
        """

        if self.error:
            raise self.error

    @override
    def __repr__(self) -> str:
        args = ",".join(f"{k}={v!r}" for k, v in sorted(self.__dict__.items()))
        return f"{self.__class__.__name__}({args})"

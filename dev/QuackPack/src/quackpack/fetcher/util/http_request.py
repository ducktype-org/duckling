"""
Module for representing and handling HTTP requests.
----
Classes:
- `HTTPRequest`: Represents an HTTP client request, including URL, method, headers, and body.
- `HTTPRequestProxy`: Combines an `HTTPRequest` object with a dictionary of defaults.
----
References:
  [1] https://github.com/tornadoweb/tornado/blob/master/tornado/httpclient.py
"""

import time
from asyncio import Future
from collections.abc import Callable
from datetime import datetime
from ssl import SSLContext
from typing import Any, Final, cast

from quackpack.fetcher.util.http_headers import HTTPHeaders


class HTTPRequest:
    """
    Represents an HTTP client request.
    ----
    Args:
    - `url`: The URL to send the request to.
    - `method`: The HTTP method to use (default: "GET").
    - `headers`: Optional dictionary or `HTTPHeaders` object containing request headers.
    - `body`: Optional bytes or string containing the request body.
    - `auth_username`: Optional username for authentication.
    - `auth_password`: Optional password for authentication.
    - `auth_mode`: Optional authentication mode.
    - `connect_timeout`: Optional float representing the connection timeout.
    - `request_timeout`: Optional float representing the request timeout.
    - `if_modified_since`: Optional float or `datetime` representing the If-Modified-Since header.
    - `follow_redirects`: Optional boolean indicating whether to follow redirects.
    - `max_redirects`: Optional integer representing the maximum number of redirects to follow.
    - `user_agent`: Optional string representing the User-Agent header.
    - `use_gzip`: Optional boolean indicating whether to use gzip compression.
    - `network_interface`: Optional string representing the network interface to use.
    - `streaming_callback`: Optional callback function for streaming data.
    - `header_callback`: Optional callback function for handling headers.
    - `prepare_curl_callback`: Optional callback function for preparing cURL options.
    - `proxy_host`: Optional string representing the proxy host.
    - `proxy_port`: Optional integer representing the proxy port.
    - `proxy_username`: Optional string representing the proxy username.
    - `proxy_password`: Optional string representing the proxy password.
    - `proxy_auth_mode`: Optional string representing the proxy authentication mode.
    - `allow_nonstandard_methods`: Optional boolean indicating whether to allow non-standard HTTP methods.
    - `validate_cert`: Optional boolean indicating whether to validate SSL certificates.
    - `ca_certs`: Optional string representing the path to CA certificates.
    - `allow_ipv6`: Optional boolean indicating whether to allow IPv6.
    - `client_key`: Optional string representing the client key.
    - `client_cert`: Optional string representing the client certificate.
    - `body_producer`: Optional function producing the request body.
    - `expect_100_continue`: Optional boolean indicating whether to expect 100 Continue.
    - `decompress_response`: Optional boolean indicating whether to decompress the response.
    - `ssl_options`: Optional dictionary or `SSLContext` containing SSL options.
    """

    _headers: dict[str, str] | HTTPHeaders | None = None

    # NOTE: Original comment.
    # Default values for HTTPRequest parameters.
    # Merged with the values on the request object by AsyncHTTPClient
    # implementations.
    DEFAULTS: Final[dict[str, Any]] = {
        "connect_timeout": 60.0,  # TODO
        "request_timeout": 60.0,  # TODO
        "follow_redirects": True,
        "max_redirects": 5,
        "decompress_response": True,
        "proxy_password": "",
        "allow_nonstandard_methods": False,
        "validate_cert": True,
    }

    def __init__(
        self,
        url: str,
        *,
        method: str = "GET",
        headers: dict[str, str] | HTTPHeaders | None = None,
        body: bytes | str | None = None,
        auth_username: str | None = None,
        auth_password: str | None = None,
        auth_mode: str | None = None,
        connect_timeout: float | None = None,
        request_timeout: float | None = None,
        if_modified_since: float | datetime | None = None,
        follow_redirects: bool | None = None,
        max_redirects: int | None = None,
        user_agent: str | None = None,
        use_gzip: bool | None = None,
        network_interface: str | None = None,
        streaming_callback: Callable[[bytes], None] | None = None,
        header_callback: Callable[[str], None] | None = None,
        prepare_curl_callback: Callable[[Any], None] | None = None,
        proxy_host: str | None = None,
        proxy_port: int | None = None,
        proxy_username: str | None = None,
        proxy_password: str | None = None,
        proxy_auth_mode: str | None = None,
        allow_nonstandard_methods: bool | None = None,
        validate_cert: bool | None = None,
        ca_certs: str | None = None,
        allow_ipv6: bool | None = None,
        client_key: str | None = None,
        client_cert: str | None = None,
        body_producer: Callable[[Callable[[bytes], None]], Future[None]] | None = None,
        expect_100_continue: bool = False,
        decompress_response: bool | None = None,
        ssl_options: dict[str, Any] | SSLContext | None = None,
    ) -> None:
        self.headers = headers
        if if_modified_since is not None:
            pass
            # NOTE: Original comment.
            # self.headers["If-Modified-Since"] = format_timestamp(
            #     if_modified_since)
        self.proxy_host = proxy_host
        self.proxy_port = proxy_port
        self.proxy_username = proxy_username
        self.proxy_password = proxy_password
        self.proxy_auth_mode = proxy_auth_mode
        self.url = url
        self.method = method
        self.body = body
        self.body_producer = body_producer
        self.auth_username = auth_username
        self.auth_password = auth_password
        self.auth_mode = auth_mode
        self.connect_timeout = connect_timeout
        self.request_timeout = request_timeout
        self.follow_redirects = follow_redirects
        self.max_redirects = max_redirects
        self.user_agent = user_agent
        self.decompress_response = decompress_response or use_gzip
        self.network_interface = network_interface
        self.streaming_callback = streaming_callback
        self.header_callback = header_callback
        if self.streaming_callback or self.header_callback:
            raise NotImplementedError
        self.prepare_curl_callback = prepare_curl_callback
        self.allow_nonstandard_methods = allow_nonstandard_methods
        self.validate_cert = validate_cert
        self.ca_certs = ca_certs
        self.allow_ipv6 = allow_ipv6
        self.client_key = client_key
        self.client_cert = client_cert
        self.ssl_options = ssl_options
        self.expect_100_continue = expect_100_continue
        self.start_time = time.time()

    @property
    def headers(self) -> HTTPHeaders:
        """
        Get the request headers.
        ----
        Returns:
        - `HTTPHeaders`: The request headers.
        """

        # NOTE: Original comment.
        # TODO: headers may actually be a plain dict until fairly late in
        # the process (AsyncHTTPClient.fetch), but practically speaking,
        # whenever the property is used they're already HTTPHeaders.
        return cast(HTTPHeaders, self._headers)

    @headers.setter
    def headers(self, value: dict[str, str] | HTTPHeaders | None) -> None:
        """
        Set the request headers.
        ----
        Args:
        - `value`: The headers to set, either as a dictionary or `HTTPHeaders` object.
        """

        if value is None:
            self._headers = HTTPHeaders()
        else:
            self._headers = value

    @property
    def body(self) -> bytes | str | None:
        """
        Get the request body.
        ----
        Returns:
        - `bytes | str | None`: The request body.
        """

        return self._body

    @body.setter
    def body(self, value: bytes | str | None) -> None:
        """
        Set the request body.
        ----
        Args:
        - `value`: The body to set, either as bytes, a string, or None.
        """

        self._body = value


class HTTPRequestProxy:
    """
    Combines an object with a dictionary of defaults.
    Used internally by `AsyncHTTPClient` implementations.
    ----
    Args:
    - `request`: The `HTTPRequest` object to proxy.
    - `defaults`: Optional dictionary of default values.
    """

    def __init__(self, request: HTTPRequest, defaults: dict[str, Any] | None) -> None:
        self.request = request
        self.defaults = defaults

    def __getattr__(self, name: str) -> Any:
        """
        Get an attribute from the proxied request or defaults.
        ----
        Args:
        - `name`: The name of the attribute to retrieve.
        ----
        Returns:
        - `Any`: The attribute value from the request or defaults.
        """

        request_attr = getattr(self.request, name) if hasattr(self.request, name) else None
        if request_attr is not None:
            return request_attr
        elif self.defaults is not None:
            return self.defaults.get(name, None)
        else:
            return None

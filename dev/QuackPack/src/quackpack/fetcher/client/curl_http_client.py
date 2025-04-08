"""
Module for handling asynchronous HTTP requests using libcurl.
----
Classes:
- `CurlError`: Exception for errors encountered by the `CurlHTTPClient`.
- `CurlInfo`: A named tuple containing information about a cURL request.
- `CurlHTTPClient`: An asynchronous HTTP client using libcurl.
----
References:
  [1] https://curl.haxx.se/libcurl/c/multi-uv.html
  [2] https://github.com/tornadoweb/tornado/blob/master/tornado/curl_httpclient.py
  [3] https://github.com/gpfei/aiocurl
"""

# NOTE: pycurl methods of Curl and MultiCurl objects like setopt, getopt have unknown types and pyright is really mad about it.
# pyright: reportUnknownMemberType = false

import asyncio
import threading
import time
from asyncio import Future, Queue
from collections.abc import Callable
from functools import partial
from io import BytesIO, FileIO
from typing import NamedTuple, cast

import pycurl

# import uvloop
from quackpack.fetcher.util import (
    HTTPError,
    HTTPHeaders,
    HTTPInputError,
    HTTPRequest,
    HTTPRequestProxy,
    HTTPResponse,
    MissingEventLoopError,
    parse_http1_response_start_line,
)
from quackpack.util.logger import get_logger

curl_log = get_logger(__name__)


class CurlError(HTTPError):
    """
    Exception for errors encountered by the `CurlHTTPClient`.
    ----
    Args:
    - `errno`: The error number from libcurl.
    - `message`: The error message from libcurl.
    """

    def __init__(self, errno: int, message: str) -> None:
        HTTPError.__init__(self, 599, message)
        self.errno = errno


class CurlInfo(NamedTuple):
    """
    A named tuple containing information about a cURL request.
    ----
    Attributes:
    - `future`: The future associated with the request.
    - `request`: The HTTP request being made.
    - `buffer`: The buffer for storing the response body.
    - `headers_buffer`: The buffer for storing the response headers.
    - `curl_start_time`: The time when the cURL request started.
    """

    future: Future[HTTPResponse]
    request: HTTPRequest
    buffer: BytesIO | FileIO
    headers_buffer: BytesIO
    curl_start_time: float


class CurlHTTPClient:
    """
    An asynchronous HTTP client using libcurl.
    ----
    Args:
    - `max_clients`: The maximum number of concurrent clients (default: 100).
    """

    def __init__(self, max_clients: int = 100) -> None:
        curl_log.debug(f"Initializing cURL with {max_clients} maximum clients")
        try:
            self._loop = asyncio.get_running_loop()
        except RuntimeError as e:
            raise MissingEventLoopError from e

        self._multi = pycurl.CurlMulti()
        self._fds: dict[int, int] = {}

        self._multi.setopt(pycurl.M_TIMERFUNCTION, self._set_timeout)
        self._multi.setopt(pycurl.M_SOCKETFUNCTION, self._handle_socket)

        # TODO: Add progress bar.
        self._curls = [self._curl_create() for _ in range(max_clients)]
        self._free_list: Queue[pycurl.Curl] = Queue()
        for _curl in self._curls:
            self._free_list.put_nowait(_curl)

        self._timeout = None
        self._closed = False

        # NOTE: Original comment.
        # libcurl has bugs that sometimes cause it to not report all
        # relevant file descriptors and timeouts to TIMERFUNCTION/
        # SOCKETFUNCTION.  Mitigate the effects of such bugs by
        # forcing a periodic scan of all active requests.
        self._force_timeout_callback = self._loop.create_task(self._periodical_force_timeout(1000.0))

        # NOTE: Original comment.
        # Work around a bug in libcurl 7.29.0: Some fields in the curl
        # multi object are initialized lazily, and its destructor will
        # segfault if it is destroyed without having been used.  Add
        # and remove a dummy handle to make sure everything is
        # initialized.
        dummy_curl_handle = pycurl.Curl()
        self._multi.add_handle(dummy_curl_handle)
        self._multi.remove_handle(dummy_curl_handle)

    def close(self) -> None:
        """
        Close the HTTP client and clean up resources.
        """
        curl_log.debug("Closing cURL client")
        if self._closed:
            return

        self._closed = True

        self._force_timeout_callback.cancel()
        if self._timeout is not None:
            self._timeout.cancel()

        for curl in self._curls:
            curl.close()
        self._multi.close()

    def _set_timeout(self, milliseconds: float) -> None:
        """
        Set a timeout for the next cURL operation.
        ----
        Args:
        - `milliseconds`: The timeout duration in milliseconds.
        """

        curl_log.debug(f"Setting cURL timeout to {milliseconds}ms")
        if self._timeout is not None:
            self._timeout.cancel()

        self._timeout = self._loop.call_at(self._loop.time() + milliseconds / 1000.0, self._handle_timeout)

    def _handle_timeout(self) -> None:
        """
        Handle a timeout event for cURL operations.
        """

        self._timeout = None
        while True:
            try:
                ret, _num_handles = self._multi.socket_action(pycurl.SOCKET_TIMEOUT, 0)
            except pycurl.error as e:
                curl_log.debug("cURL: multi socket action error")
                ret = e.args[0]

            if ret != pycurl.E_CALL_MULTI_PERFORM:
                break

        self._check_multi_info()

        # NOTE: Original comment.
        # In theory, we shouldn't have to do this because curl will
        # call _set_timeout whenever the timeout changes.  However,
        # sometimes after _handle_timeout we will need to reschedule
        # immediately even though nothing has changed from curl's
        # perspective.  This is because when socket_action is
        # called with SOCKET_TIMEOUT, libcurl decides internally which
        # timeouts need to be processed by using a monotonic clock
        # (where available) while tornado uses python's time.time()
        # to decide when timeouts have occurred.  When those clocks
        # disagree on elapsed time (as they will whenever there is an
        # NTP adjustment), tornado might call _handle_timeout before
        # libcurl is ready.  After each timeout, resync the scheduled
        # timeout with libcurl's current state.
        new_timeout = self._multi.timeout()
        if new_timeout >= 0:
            self._set_timeout(new_timeout)

    def _check_multi_info(self) -> None:
        """
        Check for completed cURL operations and process their results.
        """

        while True:
            num_q, ok_list, err_list = self._multi.info_read()
            for curl in ok_list:
                self._finish(curl)
            for curl, errnum, errmsg in err_list:
                self._finish(curl, errnum, errmsg)
            if num_q == 0:
                break

    async def _periodical_force_timeout(self, milliseconds: float) -> None:
        """
        Periodically force a timeout check for cURL operations.
        ----
        Args:
        - `milliseconds`: The interval between checks in milliseconds.
        """

        while True:
            self._handle_force_timeout()
            await asyncio.sleep(milliseconds)

    def _handle_force_timeout(self) -> None:
        """
        Force a timeout check for cURL operations.
        """

        while True:
            try:
                ret, _num_handles = self._multi.socket_all()
            except pycurl.error as e:
                ret = e.args[0]
            if ret != pycurl.E_CALL_MULTI_PERFORM:
                break

        self._check_multi_info()

    def _handle_socket(self, event: int, fd: int, _multi: pycurl.CurlMulti, _data: bytes) -> None:
        """
        Handle socket events for cURL operations.
        ----
        Args:
        - `event`: The socket event type.
        - `fd`: The file descriptor for the socket.
        - `_multi`: The cURL multi handle.
        - `_data`: Additional data associated with the event.
        """

        # TODO: investigate why OSError can be sometimes observed in stdout (but does not break anything)
        if event in (pycurl.POLL_IN, pycurl.POLL_OUT, pycurl.POLL_INOUT):
            if fd in self._fds:
                self._loop.remove_reader(fd)
                self._loop.remove_writer(fd)
            self._loop.add_reader(fd, self._handle_events, fd, event)
            self._loop.add_writer(fd, self._handle_events, fd, event)
            self._fds[fd] = event
        elif event == pycurl.POLL_REMOVE:
            if fd in self._fds:
                self._loop.remove_reader(fd)
                self._loop.remove_writer(fd)
                del self._fds[fd]

    def _handle_events(self, fd: int, events: int) -> None:
        """
        Handle socket events for cURL operations.
        ----
        Args:
        - `fd`: The file descriptor for the socket.
        - `events`: The events to handle.
        """

        action = 0
        if events & pycurl.POLL_IN or events & pycurl.POLL_INOUT:
            action |= pycurl.CSELECT_IN
        if events & pycurl.POLL_OUT or events & pycurl.POLL_INOUT:
            action |= pycurl.CSELECT_OUT

        while True:
            try:
                ret, _num_handles = self._multi.socket_action(fd, action)
            except pycurl.error as e:
                ret = e.args[0]

            if ret != pycurl.E_CALL_MULTI_PERFORM:
                break

        self._check_multi_info()

    def _finish(
        self, curl: pycurl.Curl, curl_error: int | None = None, curl_message: str | None = None
    ) -> None:
        """
        Finish a cURL operation and process the result.
        ----
        Args:
        - `curl`: The cURL handle.
        - `curl_error`: The error code, if any.
        - `curl_message`: The error message, if any.
        """

        curl_log.debug("Finishing cURL client")
        info = cast(CurlInfo, curl.info)  # pyright: ignore[reportAttributeAccessIssue]
        assert isinstance(info, CurlInfo)
        curl.info = None  # pyright: ignore[reportAttributeAccessIssue]
        self._multi.remove_handle(curl)
        self._free_list.put_nowait(curl)
        buffer = info.buffer
        headers_buffer = info.headers_buffer
        if curl_error:
            assert curl_message is not None
            error = CurlError(curl_error, curl_message)
            assert error is not None
            code = error.code
            effective_url = None
        else:
            error = None
            code = cast(int, curl.getinfo(pycurl.HTTP_CODE))
            effective_url = cast(str, curl.getinfo(pycurl.EFFECTIVE_URL))
            buffer.seek(0)

        time_info: dict[str, float] = {
            "queue": info.curl_start_time - info.request.start_time,
            "namelookup": curl.getinfo(pycurl.NAMELOOKUP_TIME),
            "connect": curl.getinfo(pycurl.CONNECT_TIME),
            "pretransfer": curl.getinfo(pycurl.PRETRANSFER_TIME),
            "starttransfer": curl.getinfo(pycurl.STARTTRANSFER_TIME),
            "total": curl.getinfo(pycurl.TOTAL_TIME),
            "redirect": curl.getinfo(pycurl.REDIRECT_TIME),
        }

        try:
            result = HTTPResponse(
                request=info.request,
                code=code,
                headers=HTTPHeaders.parse(headers_buffer.getvalue().decode()),
                buffer=buffer if isinstance(buffer, BytesIO) else None,
                effective_url=effective_url,
                error=error,
                request_time=time.time() - info.curl_start_time,
                time_info=time_info,
            )
            info.future.set_result(result)
        except Exception:
            curl_log.error("Failed to run callback.")
        finally:
            headers_buffer.close()

    async def fetch(self, request: HTTPRequest, buffer: BytesIO | FileIO) -> HTTPResponse | None:
        """
        Fetch an HTTP request using cURL.
        ----
        Args:
        - `request`: The HTTP request to fetch.
        - `buffer`: Buffer to which response body will be saved.
        ----
        Returns:
        - `HTTPResponse | None`: A HTTP response, or None if the request fails.
        """

        curl_log.debug(f"Fetching cURL {request}")

        headers_buffer = BytesIO()
        future = self._loop.create_future()

        start_time = time.monotonic()
        curl = await self._free_list.get()
        curl.info = CurlInfo(future, request, buffer, headers_buffer, start_time)  # pyright: ignore[reportAttributeAccessIssue]

        request_proxy = HTTPRequestProxy(request, dict(HTTPRequest.DEFAULTS))
        request = cast(HTTPRequest, request_proxy)

        try:
            self._curl_setup_request(curl, request, curl.info.buffer, curl.info.headers_buffer)  # pyright: ignore[reportAttributeAccessIssue]
        except Exception as e:
            curl_log.error("Failed to setup request.")
            res = HTTPResponse(request=request, code=599, error=e)
            future.set_result(res)
            await self._free_list.put(curl)
        else:
            self._multi.add_handle(curl)
            self._set_timeout(0)

        try:
            return await future
        except Exception:
            future.cancel()
            return None

    @classmethod
    def _curl_create(cls) -> pycurl.Curl:
        curl = pycurl.Curl()

        curl.setopt(pycurl.PROTOCOLS, pycurl.PROTO_HTTP | pycurl.PROTO_HTTPS)
        curl.setopt(pycurl.REDIR_PROTOCOLS, pycurl.PROTO_HTTP | pycurl.PROTO_HTTPS)

        return curl

    def _curl_setup_request(
        self, curl: pycurl.Curl, request: HTTPRequest, buffer: BytesIO | FileIO, headers_buffer: BytesIO
    ) -> None:
        """
        Set up a cURL request with the specified options.
        ----
        Args:
        - `curl`: The cURL handle.
        - `request`: The HTTP request to set up.
        - `buffer`: The buffer for storing the response body.
        - `headers_buffer`: The buffer for storing the response headers.
        """

        curl.setopt(pycurl.URL, request.url.encode())
        curl.setopt(pycurl.WRITEFUNCTION, buffer.write)

        # NOTE: Original comment.
        # libcurl's magic "Expect: 100-continue" behavior causes delays
        # with servers that don't support it (which include, among others,
        # Google's OpenID endpoint).  Additionally, this behavior has
        # a bug in conjunction with the curl_multi_socket_action API
        # (https://sourceforge.net/tracker/?func=detail&atid=100976&aid=3039744&group_id=976),
        # which increases the delays.  It's more trouble than it's worth,
        # so just turn off the feature (yes, setting Expect: to an empty
        # value is the official way to disable this)
        if "Expect" not in request.headers:
            request.headers["Expect"] = ""

        # NOTE: Original comment.
        # libcurl adds Pragma: no-cache by default; disable that too
        if "Pragma" not in request.headers:
            request.headers["Pragma"] = ""

        curl.setopt(pycurl.HTTPHEADER, [f"{k}: {v}".encode() for k, v in request.headers.items()])

        curl.setopt(
            pycurl.HEADERFUNCTION,
            partial(self._curl_header_callback, headers_buffer, request.header_callback),
        )
        curl.setopt(pycurl.FOLLOWLOCATION, request.follow_redirects)
        curl.setopt(pycurl.MAXREDIRS, request.max_redirects)
        assert request.connect_timeout is not None
        curl.setopt(pycurl.CONNECTTIMEOUT_MS, int(1000 * request.connect_timeout))
        assert request.request_timeout is not None
        curl.setopt(pycurl.TIMEOUT_MS, int(1000 * request.request_timeout))
        if request.user_agent:
            curl.setopt(pycurl.USERAGENT, request.user_agent.encode())
        else:
            curl.setopt(pycurl.USERAGENT, b"Mozilla/5.0 (compatible; aiocurl)")
        if request.network_interface:
            curl.setopt(pycurl.INTERFACE, request.network_interface)
        if request.decompress_response:
            curl.setopt(pycurl.ENCODING, b"gzip,deflate")
        else:
            curl.setopt(pycurl.ENCODING, b"none")

        if request.proxy_host and request.proxy_port:
            curl.setopt(pycurl.PROXY, request.proxy_host.encode())
            curl.setopt(pycurl.PROXYPORT, request.proxy_port)
            if request.proxy_username:
                credentials = f"{request.proxy_username}:{request.proxy_password}"
                curl.setopt(pycurl.PROXYUSERPWD, credentials.encode())

            if request.proxy_auth_mode is None or request.proxy_auth_mode == "basic":
                curl.setopt(pycurl.PROXYAUTH, pycurl.HTTPAUTH_BASIC)
            elif request.proxy_auth_mode == "digest":
                curl.setopt(pycurl.PROXYAUTH, pycurl.HTTPAUTH_DIGEST)
            else:
                raise ValueError(f"Unsupported proxy_auth_mode {request.proxy_auth_mode}")
        else:
            curl.setopt(pycurl.PROXY, b"")
            curl.unsetopt(pycurl.PROXYUSERPWD)
        if request.validate_cert:
            curl.setopt(pycurl.SSL_VERIFYPEER, 1)
            curl.setopt(pycurl.SSL_VERIFYHOST, 2)
        else:
            curl.setopt(pycurl.SSL_VERIFYPEER, 0)
            curl.setopt(pycurl.SSL_VERIFYHOST, 0)
        if request.ca_certs is not None:
            curl.setopt(pycurl.CAINFO, request.ca_certs)
        else:
            # NOTE: Original comment.
            # There is no way to restore pycurl.CAINFO to its default value
            # (Using unsetopt makes it reject all certificates).
            # I don't see any way to read the default value from python so it
            # can be restored later.  We'll have to just leave CAINFO untouched
            # if no ca_certs file was specified, and require that if any
            # request uses a custom ca_certs file, they all must.
            pass

        if request.allow_ipv6 is not None and not request.allow_ipv6:
            # NOTE: Original comment.
            # Curl behaves reasonably when DNS resolution gives an ipv6 address
            # that we can't reach, so allow ipv6 unless the user asks to disable.
            curl.setopt(pycurl.IPRESOLVE, pycurl.IPRESOLVE_V4)
        else:
            curl.setopt(pycurl.IPRESOLVE, pycurl.IPRESOLVE_WHATEVER)

        # NOTE: Original comment.
        # Set the request method through curl's irritating interface which makes
        # up names for almost every single method
        curl_options = {
            "GET": pycurl.HTTPGET,
            "POST": pycurl.POST,
            "PUT": pycurl.UPLOAD,
            "HEAD": pycurl.NOBODY,
        }
        custom_methods = {"DELETE", "OPTIONS", "PATCH", "TRACE"}
        for o in curl_options.values():
            curl.setopt(o, False)
        if request.method in curl_options:
            curl.unsetopt(pycurl.CUSTOMREQUEST)
            curl.setopt(curl_options[request.method], True)
        elif request.allow_nonstandard_methods or (request.method in custom_methods):
            curl.setopt(pycurl.CUSTOMREQUEST, request.method)
        else:
            raise KeyError("unknown method " + request.method)

        body_expected = request.method in ("POST", "PATCH", "PUT")
        body_present = request.body is not None
        if not request.allow_nonstandard_methods and body_expected ^ body_present:
            # NOTE: Original comment.
            # Some HTTP methods nearly always have bodies while others
            # almost never do. Fail in this case unless the user has
            # opted out of sanity checks with allow_nonstandard_methods.
            raise ValueError(
                f"Body must {'not ' if body_expected else ''}be None for method {request.method} (unless allow_nonstandard_methods is true)"
            )

        if body_expected or body_present:
            if request.method == "GET":
                # NOTE: Original comment.
                # Even with `allow_nonstandard_methods` we disallow
                # GET with a body (because libcurl doesn't allow it
                # unless we use CUSTOMREQUEST). While the spec doesn't
                # forbid clients from sending a body, it arguably
                # disallows the server from doing anything with them.
                raise ValueError("Body must be None for GET request")
            assert request.body is None or isinstance(request.body, str)
            body = (request.body or "").encode()
            request_buffer = BytesIO(body)

            def ioctl(cmd: int) -> None:
                if cmd == curl.IOCMD_RESTARTREAD:  # pyright: ignore[reportAttributeAccessIssue]
                    request_buffer.seek(0)

            curl.setopt(pycurl.READFUNCTION, request_buffer.read)
            curl.setopt(pycurl.IOCTLFUNCTION, ioctl)
            if request.method == "POST":
                curl.setopt(pycurl.POSTFIELDSIZE, len(body))
            else:
                curl.setopt(pycurl.UPLOAD, True)
                curl.setopt(pycurl.INFILESIZE, len(body))

        if request.auth_username is not None:
            userpwd = f"{request.auth_username}:{request.auth_password or ''}"

            if request.auth_mode is None or request.auth_mode == "basic":
                curl.setopt(pycurl.HTTPAUTH, pycurl.HTTPAUTH_BASIC)
            elif request.auth_mode == "digest":
                curl.setopt(pycurl.HTTPAUTH, pycurl.HTTPAUTH_DIGEST)
            else:
                raise ValueError(f"Unsupported auth_mode {request.auth_mode}")

            curl.setopt(pycurl.USERPWD, userpwd.encode())
            curl_log.debug(f"{request.method} {request.url} (username: {request.auth_username})")
        else:
            curl.unsetopt(pycurl.USERPWD)
            curl_log.debug(f"{request.method} {request.url}")

        if request.client_cert is not None:
            curl.setopt(pycurl.SSLCERT, request.client_cert)

        if request.client_key is not None:
            curl.setopt(pycurl.SSLKEY, request.client_key)

        if request.ssl_options is not None:
            raise ValueError("ssl_options not supported in curl_httpclient")

        if threading.active_count() > 1:
            # NOTE: Original comment.
            # libcurl/pycurl is not thread-safe by default.  When multiple threads
            # are used, signals should be disabled.  This has the side effect
            # of disabling DNS timeouts in some environments (when libcurl is
            # not linked against ares), so we don't do it when there is only one
            # thread.  Applications that use many short-lived threads may need
            # to set NOSIGNAL manually in a prepare_curl_callback since
            # there may not be any other threads running at the time we call
            # threading.activeCount.
            curl.setopt(pycurl.NOSIGNAL, 1)
        if request.prepare_curl_callback is not None:
            request.prepare_curl_callback(curl)

    def _curl_header_callback(
        self, headers_buffer: BytesIO, header_callback: Callable[[str], None] | None, header_line: bytes
    ) -> None:
        """
        Handle a header line received from a cURL request.
        ----
        Args:
        - `headers_buffer`: The buffer for storing the response headers.
        - `header_callback`: Optional callback function for handling headers.
        - `header_line`: The header line received from cURL.
        """

        if header_callback is not None:
            self._loop.call_soon(header_callback, header_line.decode())
        # NOTE: Original comment.
        # header_line as returned by curl includes the end-of-line characters.
        # whitespace at the start should be preserved to allow multi-line headers
        header_line = header_line.rstrip()
        if header_line.startswith(b"HTTP/"):
            # NOTE: Original comment.
            # clear headers buffer
            headers_buffer.seek(0)
            headers_buffer.truncate()
            try:
                (_, _, reason) = parse_http1_response_start_line(header_line.decode("latin-1"))
                header_line = f"X-Http-Reason: {reason}".encode()
            except HTTPInputError:
                return
        if not header_line:
            return
        headers_buffer.write(header_line)

from io import BytesIO

import pytest

from quackpack.core.fetcher.util import (
    HTTPError,
    HTTPHeaders,
    HTTPInputError,
    ResponseStartLine,
    parse_http1_response_start_line,
)
from quackpack.core.fetcher.util.http_request import HTTPRequest, HTTPRequestProxy
from quackpack.core.fetcher.util.http_response import HTTPResponse


class TestHTTPError:
    def test_str_code_only(self):
        error_code = 599
        error = HTTPError(code=error_code)
        assert error.code == 599
        assert error.message is None
        assert str(error) == "HTTP 599: None"

    def test_str(self):
        error_code = 404
        message = "Not Found"
        error = HTTPError(code=error_code, message=message)
        assert error.message == "Not Found"
        assert str(error) == "HTTP 404: Not Found"


class TestHTTPHeaders:
    def test_add_and_get(self):
        headers = HTTPHeaders()
        headers.add("Content-Type", "application/json")
        assert headers["Content-Type"] == "application/json"
        assert headers.get_list("Content-Type") == ["application/json"]

    def test_add_multiple_values_for_key(self):
        headers = HTTPHeaders()
        headers.add("Content-Type", "application/json")
        headers.add("Content-Type", "application/xml")
        assert headers["Content-Type"] == "application/json,application/xml"
        assert headers.get_list("Content-Type") == [
            "application/json",
            "application/xml",
        ]

    def test_parse_line_valid(self):
        headers = HTTPHeaders()
        headers.parse_line("Content-Type: application/json")
        assert headers["Content-Type"] == "application/json"

    def test_parse_line_invalid(self):
        headers = HTTPHeaders()
        with pytest.raises(HTTPInputError):
            headers.parse_line("InvalidHeaderWithoutColon")

    def test_parse_valid_headers(self):
        headers_text = "Content-Type: application/json\r\nUser-Agent: pytest\r\n"
        headers = HTTPHeaders.parse(headers_text)
        assert headers["Content-Type"] == "application/json"
        assert headers["User-Agent"] == "pytest"

    def test_parse_invalid_headers(self):
        headers_text = "InvalidHeader\r\nContent-Type application/json\r\n"
        with pytest.raises(HTTPInputError):
            HTTPHeaders.parse(headers_text)

    def test_parse_headers_leading_space(self):
        headers_text_leading_space = " Content-Type: application/json\r\n"
        with pytest.raises(HTTPInputError):
            HTTPHeaders.parse(headers_text_leading_space)

        headers_text = "SomeMultiLineHeader: part1\r\n part2\r\n"
        headers = HTTPHeaders.parse(headers_text)
        assert headers["SomeMultiLineHeader"] == "part1 part2"

    def test_copy(self):
        headers = HTTPHeaders()
        headers.add("Content-Type", "application/json")

        copied_headers = headers.copy()
        assert copied_headers["Content-Type"] == "application/json"

    def test_len(self):
        headers = HTTPHeaders()
        headers.add("Content-Type", "application/json")
        assert len(headers) == 1
        headers.add("User-Agent", "pytest")
        assert len(headers) == 2
        headers.add("Content-Type", "application/html")
        assert len(headers) == 2

    def test_del(self):
        headers = HTTPHeaders()
        headers.add("Content-Type", "application/json")
        assert headers["Content-Type"] == "application/json"
        del headers["Content-Type"]
        assert len(headers) == 0

    def test_iter(self):
        headers = HTTPHeaders()
        headers.add("a", "1")
        headers.add("b", "2")
        headers.add("c", "3")
        assert list(headers) == ["A", "B", "C"]

    def test_str(self):
        headers = HTTPHeaders()
        headers.add("Content-Type", "application/json")
        headers.add("User-Agent", "pytest")

        expected_str = "Content-Type: application/json\nUser-Agent: pytest\n"
        assert str(headers) == expected_str


class TestResponseStartLine:
    def test_parse_valid_start_line(self):
        line = "HTTP/1.1 200 OK"
        result = parse_http1_response_start_line(line)
        assert result == ResponseStartLine(version="HTTP/1.1", code=200, reason="OK")

    def test_parse_invalid_start_line(self):
        line = "Invalid Start Line"

        with pytest.raises(HTTPInputError):
            parse_http1_response_start_line(line)


class TestHTTPRequest:
    def test_init_with_custom_values(self):
        headers = HTTPHeaders()
        headers.add("User-Agent", "pytest")
        request = HTTPRequest(
            url="https://example.com",
            method="POST",
            headers=headers,
            body="Test Body",
            connect_timeout=30.0,
            request_timeout=30.0,
            follow_redirects=False,
            max_redirects=3,
            if_modified_since=10.0,
        )

        assert request.method == "POST"
        assert request.headers["User-Agent"] == "pytest"
        assert request.body == "Test Body"
        assert request.connect_timeout == 30.0
        assert request.request_timeout == 30.0
        assert request.follow_redirects is False
        assert request.max_redirects == 3

        assert not hasattr(request, "if_modified_since")

    def test_headers_setter_getter(self):
        headers = HTTPHeaders()
        headers.add("Content-Type", "application/json")

        request = HTTPRequest(url="https://example.com", headers=headers)
        assert request.headers["Content-Type"] == "application/json"

        new_headers = HTTPHeaders()
        new_headers.add("Accept", "application/xml")
        request.headers = new_headers
        assert request.headers["Accept"] == "application/xml"

    def test_body_setter_getter(self):
        request = HTTPRequest(url="https://example.com", body="Test Body")
        assert request.body == "Test Body"
        request.body = "New Body"
        assert request.body == "New Body"

    def test_invalid_streaming_or_header_callback(self):
        with pytest.raises(NotImplementedError):
            HTTPRequest(url="https://example.com", streaming_callback=lambda _: None)


class TestHTTPRequestProxy:
    def test_getattr_from_request(self):
        request = HTTPRequest(url="https://example.com", method="POST")
        proxy = HTTPRequestProxy(request=request, defaults=None)
        assert proxy.url == "https://example.com"
        assert proxy.method == "POST"

    def test_getattr_from_defaults(self):
        request = HTTPRequest(url="https://example.com")
        defaults = {"method": "GET", "connect_timeout": 100}
        proxy = HTTPRequestProxy(request=request, defaults=defaults)
        assert proxy.method == "GET"
        assert proxy.connect_timeout == 100
        assert proxy.non_existing_attribute is None

    def test_getattr_none_if_not_in_request_or_defaults(self):
        request = HTTPRequest(url="https://example.com")
        proxy = HTTPRequestProxy(request=request, defaults=None)
        assert proxy.non_existing_attribute is None


class TestHTTPResponse:
    def test_init_with_default_values(self):
        request = HTTPRequest(url="https://example.com")

        response = HTTPResponse(request=request, code=200)

        assert response.request == request
        assert response.code == 200
        assert response.raw_headers == HTTPHeaders()
        assert response.effective_url == "https://example.com"
        assert response.error is None
        assert response.start_time is None
        assert response.request_time is None
        assert response.time_info == {}

    def test_init_with_custom_values(self):
        headers = HTTPHeaders()
        headers.add("Content-Type", "application/json")

        buffer = BytesIO(b"Test body")
        request = HTTPRequest(url="https://example.com")

        response = HTTPResponse(
            request=request,
            code=404,
            headers=headers,
            buffer=buffer,
            effective_url="https://example.com/404",
            request_time=1.5,
            time_info={"connect": 0.5},
            reason="Not Found",
            start_time=1615500000.0,
        )

        assert response.code == 404
        assert response.raw_headers["Content-Type"] == "application/json"
        assert response.effective_url == "https://example.com/404"
        assert response.request_time == 1.5
        assert response.time_info == {"connect": 0.5}
        assert response.reason == "Not Found"
        assert response.start_time == 1615500000.0

    def test_body_property(self):
        buffer = BytesIO(b"Test body content")
        request = HTTPRequest(url="https://example.com")
        response = HTTPResponse(request=request, code=200, buffer=buffer)
        assert response.body == b"Test body content"
        response = HTTPResponse(request=request, code=200)
        assert response.body == b""

    def test_rethrow_no_error(self):
        request = HTTPRequest(url="https://example.com")
        response = HTTPResponse(request=request, code=200)

        response.rethrow()

    def test_rethrow_with_error(self):
        request = HTTPRequest(url="https://example.com")
        response = HTTPResponse(request=request, code=404, reason="Not Found")

        with pytest.raises(HTTPError):
            response.rethrow()

    def test_repr(self):
        request = HTTPRequest(url="https://example.com")
        response = HTTPResponse(request=request, code=200)

        repr_str = repr(response)
        assert "HTTPResponse" in repr_str
        assert "request=" in repr_str
        assert "code=200" in repr_str

    def test_request_proxy(self):
        request = HTTPRequest(url="https://example.com")
        proxy = HTTPRequestProxy(request=request, defaults=None)
        response = HTTPResponse(request=proxy, code=200)
        assert response.request == request

    def test_error(self):
        request = HTTPRequest(url="https://example.com")
        response = HTTPResponse(request=request, code=100)
        assert isinstance(response.error, HTTPError) and response.error.code == 100

        error = HTTPError(code=123)
        response = HTTPResponse(request=request, code=200, error=error)
        assert isinstance(response.error, HTTPError) and response.error.code == 123

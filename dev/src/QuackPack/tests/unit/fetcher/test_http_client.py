# ruff: noqa: ARG002

import asyncio
import os
import socket
from io import BytesIO

import pytest
import pytest_asyncio
from ducknest import FastAPIFactory
from fastapi import FastAPI
from hypercorn.config import Config

from quackpack.core.fetcher.client.curl_http_client import CurlHTTPClient
from quackpack.core.fetcher.util import HTTPRequest
from quackpack.util.types.errors import QuackPackError


# NOTE: Beauties of using non-isolated CI/CD runner...
@pytest.fixture(scope="module")
def tcp_port():
    tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp.bind(("", 0))
    _, port = tcp.getsockname()
    tcp.close()
    yield port


# NOTE: Just usual pytest_asyncio magic - *simply* override event_loop_policy fixture.
@pytest.fixture(scope="module")  # pyright: ignore[reportArgumentType]
def event_loop_policy():
    policy = asyncio.DefaultEventLoopPolicy()
    if os.name == "posix":
        try:
            import uvloop

            policy = uvloop.EventLoopPolicy()
        except ImportError:
            pass
    return policy


def test_http_client_no_event_loop():
    with pytest.raises(QuackPackError):
        _http_client = CurlHTTPClient()


# NOTE: All tests within this class will (of course) use the same event loop.
#       Discovery of this feature (and debugging it) took another 1 hour...
@pytest.mark.asyncio(loop_scope="class")
class TestHTTPClient:
    # NOTE: Discovery of pytest_asyncio.fixture took about 2 hours...
    #       Of course, it is required - async fixtures do not work otherwise.
    #       Oh, by the way - scope and loop_scope are two different things...
    @pytest_asyncio.fixture(loop_scope="class", scope="class")
    async def dummy_app(self, tcp_port: int):
        from ducknest.routers import dummy_routers

        hypercorn_config = Config()
        hypercorn_config.bind = [f"localhost:{tcp_port}"]

        factory = FastAPIFactory(hypercorn_config=hypercorn_config)

        app = factory.create_app(router=dummy_routers.router)

        await factory.start_server()
        # NOTE: Just don't ask...
        await asyncio.sleep(1)
        yield app
        await factory.stop_server()

    @pytest_asyncio.fixture(loop_scope="class")
    async def http_client(self):
        client = CurlHTTPClient(max_clients=10)
        yield client
        client.close()

    async def test_double_close(self):
        http_client = CurlHTTPClient()
        http_client.close()
        http_client.close()

    async def test_error(self, http_client: CurlHTTPClient, dummy_app: FastAPI, tcp_port: int):
        request = HTTPRequest(url="http://localhost:0/dummy/hello", method="GET")

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert response.code == 599

    async def test_single_request(self, http_client: CurlHTTPClient, dummy_app: FastAPI, tcp_port: int):
        request = HTTPRequest(url=f"http://localhost:{tcp_port}/dummy/hello", method="GET")

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)

            assert response is not None
            assert response.code == 200
            assert response.buffer is not None
            assert response.buffer.read().decode() == '{"msg":"Hello World"}'

    async def test_multiple_requests(self, http_client: CurlHTTPClient, dummy_app: FastAPI, tcp_port: int):
        with BytesIO() as buf:
            request = HTTPRequest(url=f"http://localhost:{tcp_port}/dummy/hello", method="GET")
            response = await http_client.fetch(request, buf)

            assert response is not None
            assert response.code == 200
            assert response.buffer is not None
            assert response.buffer.read().decode() == '{"msg":"Hello World"}'

            request = HTTPRequest(url=f"http://localhost:{tcp_port}/dummy/hello1", method="GET")
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert response.code == 404

    async def test_file_buffer(
        self, http_client: CurlHTTPClient, dummy_app: FastAPI, monkeypatch: pytest.MonkeyPatch, tcp_port: int
    ):
        print(type(monkeypatch))
        mock_file = BytesIO()

        def mock_open(file: str, mode: str):
            assert file == "dummy_file.txt"
            assert mode == "w"
            return mock_file

        monkeypatch.setattr("builtins.open", mock_open)

        request = HTTPRequest(url=f"http://localhost:{tcp_port}/dummy/hello", method="GET")

        response = await http_client.fetch(request, mock_file)
        assert response is not None

        mock_file.seek(0)
        file_content = mock_file.read().decode()

        assert file_content == '{"msg":"Hello World"}'

    async def test_post(self, http_client: CurlHTTPClient, dummy_app: FastAPI, tcp_port: int):
        with BytesIO() as buf:
            request = HTTPRequest(
                url=f"http://localhost:{tcp_port}/dummy/echo",
                method="POST",
                body='{"name":"abc"}',
                headers={"content-type": "application/json"},
            )
            response = await http_client.fetch(request, buf)

            assert response is not None
            assert response.code == 200
            assert response.buffer is not None
            assert response.buffer.read().decode() == "abc"

    async def test_request_body(self, http_client: CurlHTTPClient, dummy_app: FastAPI, tcp_port: int):
        with BytesIO() as buf:
            request = HTTPRequest(url=f"http://localhost:{tcp_port}/dummy/echo", method="POST", body=None)
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert isinstance(response.error, ValueError)

            request = HTTPRequest(url=f"http://localhost:{tcp_port}/dummy/echo", method="GET", body="abc")
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert isinstance(response.error, ValueError)

            request = HTTPRequest(url=f"http://localhost:{tcp_port}/dummy/echo", method="PATCH", body="abc")
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert response.code == 405

    async def test_parallel_requests(self, http_client: CurlHTTPClient, dummy_app: FastAPI, tcp_port: int):
        secs = 2

        requests = [HTTPRequest(url=f"http://localhost:{tcp_port}/dummy/nap/{secs}") for _ in range(5)]
        tasks = [asyncio.create_task(http_client.fetch(request, BytesIO())) for request in requests]
        for task in asyncio.as_completed(tasks, timeout=secs + 0.5):
            response = await task
            assert response is not None
            assert response.code == 200

        # fixture provides curl client with 10 handlers, thus performing 11 requests requires 2 rounds and the timeout is expected
        requests = [HTTPRequest(url=f"http://localhost:{tcp_port}/dummy/nap/{secs}") for _ in range(11)]
        tasks = [asyncio.create_task(http_client.fetch(request, BytesIO())) for request in requests]
        with pytest.raises(asyncio.TimeoutError):
            for task in asyncio.as_completed(tasks, timeout=secs + 0.5):
                response = await task
                assert response is not None
                assert response.code == 200

    @pytest.mark.skip(reason="TODO: does not work on students")
    async def test_features(self, http_client: CurlHTTPClient, dummy_app: FastAPI, tcp_port: int):
        user_agent = "MyCustomUserAgent/1.0"
        custom_headers = {"X-Custom-Header": "CustomValue"}
        request = HTTPRequest(
            url=f"http://localhost:{tcp_port}/dummy/hello",
            method="GET",
            user_agent=user_agent,
            headers=custom_headers,
        )

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert response.code == 200

        proxy_host = "localhost"
        proxy_port = 8080
        request = HTTPRequest(
            url=f"http://localhost:{tcp_port}/dummy/hello",
            method="GET",
            proxy_host=proxy_host,  # assuming no proxy, this should fail
            proxy_port=proxy_port,
            proxy_username="admin",
            proxy_password="admin",
        )

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert response.code == 599

        proxy_host = "localhost"
        proxy_port = 8080
        request = HTTPRequest(
            url=f"http://localhost:{tcp_port}/dummy/hello",
            method="GET",
            proxy_host=proxy_host,  # assuming no proxy, this should fail
            proxy_port=proxy_port,
            proxy_auth_mode="digest",
        )

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert response.code == 599

        proxy_host = "localhost"
        proxy_port = 8080
        request = HTTPRequest(
            url=f"http://localhost:{tcp_port}/dummy/hello",
            method="GET",
            proxy_host=proxy_host,
            proxy_port=proxy_port,
            proxy_auth_mode="chomik dżungarski",  # this custom method should fail
        )

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert isinstance(response.error, ValueError)

        request = HTTPRequest(
            url=f"http://localhost:{tcp_port}/dummy/hello",
            method="GET",
            connect_timeout=1,
            network_interface="eth91235",  # connection on this interface should time out
        )

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert response.code == 599

        request = HTTPRequest(
            url=f"http://localhost:{tcp_port}/dummy/hello",
            method="HBDŻ",  # this custom method should fail
        )

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert isinstance(response.error, KeyError)

        request = HTTPRequest(
            url=f"http://localhost:{tcp_port}/dummy/hello",
            method="GET",
            validate_cert=False,
            decompress_response=True,
            allow_ipv6=False,
        )

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert response.code == 200

        request = HTTPRequest(
            url=f"http://localhost:{tcp_port}/dummy/hello",
            method="GET",
            auth_username="admin",
            auth_password="admin",
        )

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert response.code == 200

        request = HTTPRequest(
            url=f"http://localhost:{tcp_port}/dummy/hello",
            method="GET",
            auth_username="admin",
            auth_password="admin",
            auth_mode="digest",
        )

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert response.code == 200

        request = HTTPRequest(
            url=f"http://localhost:{tcp_port}/dummy/hello",
            method="GET",
            auth_username="admin",
            auth_password="admin",
            auth_mode="chomik dżungarski",  # this custom method should fail
        )

        with BytesIO() as buf:
            response = await http_client.fetch(request, buf)
            assert response is not None
            assert isinstance(response.error, ValueError)

        # TODO: there are more single statements to be covered, which should be tested analogously

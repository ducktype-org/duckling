# NOTE: Trust me, I know what I am doing

# ruff: noqa: PGH004
# ruff: noqa
# type: ignore

import asyncio
import os

import pytest
import pytest_asyncio
from fastapi import FastAPI
from hypercorn.config import Config

from ducknest import FastAPIFactory
from quackpack.fetcher.api_types import MultiMetadata, Package, SingleMetadata
from quackpack.fetcher.client.ducknest_client import DucknestClientContext
from quackpack.fetcher.fetcher import DucknestClient
from quackpack.util.errors import QuackPackError
from test_http_client import tcp_port
from quackpack.util.pkgid import Identifier


@pytest.fixture(scope="class")
def static_file_structure(tmp_path_factory):
    base_dir = tmp_path_factory.mktemp("xd")

    (base_dir / "static" / "bar" / "2.5.6").mkdir(parents=True, exist_ok=True)
    (base_dir / "static" / "bar" / "2.5.6" / "metadata.json").write_text(
        '{ "metadata": { "author": "Patryk Rogalski", "version": "2.5.6", "name": "bar" }, "dependencies": { "pkg1": { "version": "2.3.6" } } }'
    )
    (base_dir / "static" / "bar" / "2.5.6" / "source.tar.gz").write_bytes(b"bar-2.5.6")

    (base_dir / "static" / "foo" / "1.2.3").mkdir(parents=True, exist_ok=True)
    (base_dir / "static" / "foo" / "1.2.3" / "metadata.json").write_text(
        '{ "metadata": { "author": "Patryk Rogalski", "version": "1.2.3", "name": "foo" }, "dependencies": { "pkg2": { "version": "2.3.6" }, "pkg3": { "version": "2.4.7" } } }'
    )
    (base_dir / "static" / "foo" / "1.2.3" / "source.tar.gz").write_bytes(b"foo-1.2.3")

    (base_dir / "static" / "foo" / "1.2.5").mkdir(parents=True, exist_ok=True)
    (base_dir / "static" / "foo" / "1.2.5" / "metadata.json").write_text(
        '{ "metadata": { "author": "Patryk Rogalski", "version": "1.2.5", "name": "foo" } }'
    )
    (base_dir / "static" / "foo" / "1.2.5" / "source.tar.gz").write_bytes(b"foo-1.2.5")

    return base_dir


@pytest_asyncio.fixture(loop_scope="class", scope="class")
async def ducknest_app(static_file_structure, tcp_port):
    os.environ["FASTAPI_BASEDIR"] = str(static_file_structure)
    from ducknest.routers import packages_routers

    hypercorn_config = Config()
    hypercorn_config.bind = [f"localhost:{tcp_port}"]

    factory = FastAPIFactory(hypercorn_config=hypercorn_config)

    app = factory.create_app(router=packages_routers.router)

    await factory.start_server()
    # NOTE: Just don't ask...
    await asyncio.sleep(1)
    yield app
    await factory.stop_server()
    del os.environ["FASTAPI_BASEDIR"]


@pytest.mark.asyncio(loop_scope="class")
class TestDucknestClient:
    @pytest_asyncio.fixture(loop_scope="class")
    async def ducknest_client(self):
        client = DucknestClient()
        yield client
        client.close()

    async def test_single_metadata(self, ducknest_client: DucknestClient, ducknest_app: FastAPI, tcp_port):
        pkg = Package(id=Identifier("foo"), version="1.2.3")
        result = await ducknest_client.get_package_metadata(f"http://localhost:{tcp_port}", pkg)

        expected = '{ "metadata": { "author": "Patryk Rogalski", "version": "1.2.3", "name": "foo" }, "dependencies": { "pkg2": { "version": "2.3.6" }, "pkg3": { "version": "2.4.7" } } }'

        assert result == SingleMetadata.model_validate_json(expected)

        pkg = Package(id=Identifier("foo"), version="1.2.4")

        with pytest.raises(QuackPackError):
            result = await ducknest_client.get_package_metadata(f"http://localhost:{tcp_port}", pkg)

    async def test_multi_metadata(self, ducknest_client: DucknestClient, ducknest_app: FastAPI, tcp_port):
        pkg_name = "foo"
        result = await ducknest_client.get_package_all_metadata(f"http://localhost:{tcp_port}", pkg_name)

        expected = """{ "packages_metadata": [
            { "metadata": { "author": "Patryk Rogalski", "version": "1.2.3", "name": "foo" }, "dependencies": { "pkg2": { "version": "2.3.6" }, "pkg3": { "version": "2.4.7" } } },
            { "metadata": { "author": "Patryk Rogalski", "version": "1.2.5", "name": "foo" } }
            ]}
            """

        assert result == MultiMetadata.model_validate_json(expected)

        pkg_name = "bar"
        result = await ducknest_client.get_package_all_metadata(f"http://localhost:{tcp_port}", pkg_name)

        expected = """{ "packages_metadata": [
            { "metadata": { "author": "Patryk Rogalski", "version": "2.5.6", "name": "bar" }, "dependencies": { "pkg1": { "version": "2.3.6" } } }
            ]}
            """

        assert result == MultiMetadata.model_validate_json(expected)

        pkg_name = "baz"
        with pytest.raises(QuackPackError):
            result = await ducknest_client.get_package_all_metadata(f"http://localhost:{tcp_port}", pkg_name)

    async def test_blob(
        self, ducknest_client: DucknestClient, ducknest_app: FastAPI, static_file_structure, tcp_port
    ):
        pkg = Package(id=Identifier("bar"), version="2.5.6")
        fp = static_file_structure / "blob-bar"
        result = await ducknest_client.get_package_blob(f"http://localhost:{tcp_port}", pkg, fp)

        assert result is True
        with open(fp, "rb") as f:
            assert f.read() == b"bar-2.5.6"

        pkg = Package(id=Identifier("baz"), version="2.5.6")
        fp = static_file_structure / "blob-baz"
        result = await ducknest_client.get_package_blob(f"http://localhost:{tcp_port}", pkg, fp)

        assert result is False
        with open(fp, "rb") as f:
            assert f.read() == b""


@pytest.mark.asyncio(loop_scope="class")
class TestDucknestClientContext:
    async def test_single_metadata(self, ducknest_app: FastAPI, tcp_port):
        with DucknestClientContext() as ducknest_client:
            pkg = Package(id=Identifier("foo"), version="1.2.3")
            result = await ducknest_client.get_package_metadata(f"http://localhost:{tcp_port}", pkg)

            expected = '{ "metadata": { "author": "Patryk Rogalski", "version": "1.2.3", "name": "foo" }, "dependencies": { "pkg2": { "version": "2.3.6" }, "pkg3": { "version": "2.4.7" } } }'

            assert result == SingleMetadata.model_validate_json(expected)

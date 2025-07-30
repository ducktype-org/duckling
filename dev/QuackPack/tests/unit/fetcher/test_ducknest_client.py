# ruff: noqa: ARG002, F811

import asyncio
import os
from pathlib import Path

import pytest
import pytest_asyncio
from ducknest import FastAPIFactory
from fastapi import FastAPI
from hypercorn.config import Config
from jsons import BAR256_JSON, BAR_MUTLI, FOO123_JSON, FOO125_JSON, FOO_MULTI
from test_http_client import (
    tcp_port,  # noqa: F401  # pyright: ignore[reportUnusedImport]; needed for fixtures...
)

from quackpack.fetcher.api_types import MultiMetadata, Package, SingleMetadata
from quackpack.fetcher.client.ducknest_client import DucknestClientContext
from quackpack.fetcher.fetcher import DucknestClient
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier


@pytest.fixture(scope="class")
def static_file_structure(tmp_path_factory: pytest.TempPathFactory):
    base_dir = tmp_path_factory.mktemp("xd")

    (base_dir / "static" / "bar" / "2.5.6").mkdir(parents=True, exist_ok=True)
    (base_dir / "static" / "bar" / "2.5.6" / "metadata.json").write_text(BAR256_JSON)
    (base_dir / "static" / "bar" / "2.5.6" / "source.tar.gz").write_bytes(b"bar-2.5.6")

    (base_dir / "static" / "foo" / "1.2.3").mkdir(parents=True, exist_ok=True)
    (base_dir / "static" / "foo" / "1.2.3" / "metadata.json").write_text(FOO123_JSON)
    (base_dir / "static" / "foo" / "1.2.3" / "source.tar.gz").write_bytes(b"foo-1.2.3")

    (base_dir / "static" / "foo" / "1.2.5").mkdir(parents=True, exist_ok=True)
    (base_dir / "static" / "foo" / "1.2.5" / "metadata.json").write_text(FOO125_JSON)
    (base_dir / "static" / "foo" / "1.2.5" / "source.tar.gz").write_bytes(b"foo-1.2.5")

    return base_dir


@pytest_asyncio.fixture(loop_scope="class", scope="class")
async def ducknest_app(static_file_structure: Path, tcp_port: int):
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

    async def test_single_metadata(
        self, ducknest_client: DucknestClient, ducknest_app: FastAPI, tcp_port: int
    ):
        pkg = Package(id=Identifier("foo"), version="1.2.3")
        result = await ducknest_client.get_package_metadata(f"http://localhost:{tcp_port}", pkg)

        expected = FOO123_JSON

        assert result == SingleMetadata.model_validate_json(expected)

        pkg = Package(id=Identifier("foo"), version="1.2.4")

        with pytest.raises(QuackPackError):
            result = await ducknest_client.get_package_metadata(f"http://localhost:{tcp_port}", pkg)

    async def test_multi_metadata(
        self, ducknest_client: DucknestClient, ducknest_app: FastAPI, tcp_port: int
    ):
        pkg_name = Identifier("foo")
        result = await ducknest_client.get_package_all_metadata(f"http://localhost:{tcp_port}", pkg_name)

        assert result == MultiMetadata.model_validate_json(FOO_MULTI)

        pkg_name = Identifier("bar")
        result = await ducknest_client.get_package_all_metadata(f"http://localhost:{tcp_port}", pkg_name)

        assert result == MultiMetadata.model_validate_json(BAR_MUTLI)

        pkg_name = Identifier("baz")
        with pytest.raises(QuackPackError):
            result = await ducknest_client.get_package_all_metadata(f"http://localhost:{tcp_port}", pkg_name)

    async def test_blob(
        self,
        ducknest_client: DucknestClient,
        ducknest_app: FastAPI,
        static_file_structure: Path,
        tcp_port: int,
    ):
        pkg = Package(id=Identifier("bar"), version="2.5.6")
        fp = static_file_structure / "blob-bar"
        result = await ducknest_client.get_package_blob(f"http://localhost:{tcp_port}", pkg, fp, None)

        assert result is True
        with open(fp, "rb") as f:
            assert f.read() == b"bar-2.5.6"

        pkg = Package(id=Identifier("baz"), version="2.5.6")
        fp = static_file_structure / "blob-baz"
        result = await ducknest_client.get_package_blob(f"http://localhost:{tcp_port}", pkg, fp, None)

        assert result is False
        with open(fp, "rb") as f:
            assert f.read() == b""


@pytest.mark.asyncio(loop_scope="class")
class TestDucknestClientContext:
    async def test_single_metadata(self, ducknest_app: FastAPI, tcp_port: int):
        with DucknestClientContext() as ducknest_client:
            pkg = Package(id=Identifier("foo"), version="1.2.3")
            result = await ducknest_client.get_package_metadata(f"http://localhost:{tcp_port}", pkg)

            expected = FOO123_JSON
            assert result == SingleMetadata.model_validate_json(expected)

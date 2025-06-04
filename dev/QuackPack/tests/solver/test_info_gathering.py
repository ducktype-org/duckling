"""# ruff: noqa: PGH004
# ruff: noqa
# type: ignore

import pytest
import pytest_asyncio
from ..unit.fetcher.ducknest import FastAPIFactory
from fastapi import FastAPI
from hypercorn.config import Config

# NOTE: Beauties of using non-isolated CI/CD runner...
@pytest.fixture(scope="module")
def tcp_port():
    tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp.bind(("", 0))
    _, port = tcp.getsockname()
    tcp.close()
    yield port


# NOTE: Just usual pytest_asyncio magic - *simply* override event_loop_policy fixture.
@pytest_asyncio.fixture(scope="module")
def event_loop_policy():
    policy = asyncio.DefaultEventLoopPolicy()
    if os.name == "posix":
        try:
            import uvloop

            policy = uvloop.EventLoopPolicy()
        except ImportError:
            pass
    return policy

# Assets for tests 1 and 2

@pytest.fixture(scope="class")
def static_file_structure_min(tmp_path_factory):
    base_dir = tmp_path_factory.mktemp("xd")

    (base_dir / "static" / "package1" / "1.0.0").mkdir(parents=True, exist_ok=True)
    (base_dir / "static" / "package1" / "1.0.0" / "metadata.json").write_text(
        '{ "metadata": { "author": "Patryk Rogalski", "version": "1.0.0", "id": "package1-1.0.0", "name": "package1", "license": "GLTWSPL" }, "dependencies": { "package2": {"version": "1.0.0", "conditions": {"project_flags": "A"}, "flags": "B"} }'
    )

    (base_dir / "static" / "package2" / "1.0.0").mkdir(parents=True, exist_ok=True)
    (base_dir / "static" / "package2" / "1.0.0" / "metadata.json").write_text(
        '{ "metadata": { "author": "Patryk Rogalski", "version": "1.0.0", "id": "package2-1.0.0", "name": "package2", "license": "GLTWSPL" }'
    )

    return base_dir


@pytest_asyncio.fixture(loop_scope="class", scope="class")
async def ducknest_app(static_file_structure_min, tcp_port):
    os.environ["FASTAPI_BASEDIR"] = str(static_file_structure_min)
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
class TestOneDependency():
"""

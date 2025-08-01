import sys

from quackpack.util.env import Env


def test_basic_env():
    env = Env({"ala": "bar"})
    assert "ala" in env
    assert env["ala"] == "bar"
    if sys.platform == "win32":
        assert "aLa" in env
        assert env["aLa"] == "bar"
    else:
        assert "aLa" not in env

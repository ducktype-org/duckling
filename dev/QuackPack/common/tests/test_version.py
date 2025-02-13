import pytest

from common.version import Version


class TestVersion:
    def test_version_parsing(self):
        v = Version.create_from_string("1")
        assert v.major == 1
        assert v.minor == 0
        assert v.patch == 0
        v = Version.create_from_string("1.1")
        assert v.major == 1
        assert v.minor == 1
        assert v.patch == 0
        v = Version.create_from_string("1.1.1")
        assert v.major == 1
        assert v.minor == 1
        assert v.patch == 1
        with pytest.raises(ValueError):
            Version.create_from_string("1.1.1.0")
        with pytest.raises(ValueError):
            Version.create_from_string("1.1.1.0")
        with pytest.raises(ValueError):
            Version.create_from_string("Ala.ma.kota")
        with pytest.raises(ValueError):
            Version.create_from_string("-1")
        with pytest.raises(ValueError):
            Version(0, 0, 0)

    def test_version_compare(self):
        v = Version.create_from_string("1.2.3")
        x = Version(1, 2, 3)
        assert v == x
        x = Version(1, 2, 0)
        assert v != x
        assert x < v
        assert v > x
        assert not v.can_be_upgraded_to(x)
        assert x.can_be_upgraded_to(v)
        x = Version(21, 3, 7)
        assert v < x

    def test_satisfies(self):
        v = Version(1, 2, 3)
        x = Version(2)
        assert not x.can_be_upgraded_to(v)
        assert not v.can_be_upgraded_to(x)
        v = Version(0, 3, 7)
        x = Version(0, 4)
        assert not x.can_be_upgraded_to(v)
        assert not v.can_be_upgraded_to(x)
        x = Version(0, 3, 6)
        assert not v.can_be_upgraded_to(x)
        assert x.can_be_upgraded_to(v)

import pytest

from quackpack.util.version import Version


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
        with pytest.raises(ValueError) as excinfo:
            Version.create_from_string("1.1.1.0")
        assert str(excinfo.value) == "Too many arguments"
        with pytest.raises(ValueError) as excinfo:
            Version.create_from_string("1.1.")
        assert str(excinfo.value) == "invalid literal for int() with base 10: ''"
        with pytest.raises(ValueError) as excinfo:
            Version.create_from_string("Ala.ma.kota")
        assert str(excinfo.value) == "invalid literal for int() with base 10: 'Ala'"
        with pytest.raises(ValueError) as excinfo:
            Version.create_from_string("-1")
        assert str(excinfo.value) == "Negative versions are not supported"
        with pytest.raises(ValueError) as excinfo:
            Version(0, 0, 0)
        assert str(excinfo.value) == "Versions of format 0.0.X are not valid SemVer"
        with pytest.raises(ValueError) as excinfo:
            Version(0, 0, 1)
        assert str(excinfo.value) == "Versions of format 0.0.X are not valid SemVer"

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

    def test_update_to_positive_major(self):
        v = Version(1, 2, 3)
        x = Version(2)
        assert not x.can_be_upgraded_to(v)
        assert not v.can_be_upgraded_to(x)
        assert v.can_be_upgraded_to(v)
        assert x.can_be_upgraded_to(x)
        x = Version(1, 2, 2)
        assert x.can_be_upgraded_to(v)
        assert not v.can_be_upgraded_to(x)
        assert x.can_be_upgraded_to(x)
        x = Version(1, 2, 4)
        assert v.can_be_upgraded_to(x)
        assert not x.can_be_upgraded_to(v)
        assert x.can_be_upgraded_to(x)
        x = v.bump_minor()
        assert v.can_be_upgraded_to(x)
        assert not x.can_be_upgraded_to(v)
        assert x.can_be_upgraded_to(x)

    def test_update_to_zero_major(self):
        v = Version(0, 3, 7)
        x = Version(0, 4)
        assert not x.can_be_upgraded_to(v)
        assert not v.can_be_upgraded_to(x)
        assert v.can_be_upgraded_to(v)
        assert x.can_be_upgraded_to(x)
        x = Version(0, 3, 6)
        assert not v.can_be_upgraded_to(x)
        assert x.can_be_upgraded_to(v)
        assert x.can_be_upgraded_to(x)
        x = Version(0, 2)
        assert not x.can_be_upgraded_to(v)
        assert not v.can_be_upgraded_to(x)
        assert x.can_be_upgraded_to(x)

    def test_bump(self):
        v = Version(1, 2, 3)
        assert v.bump_patch() == Version(1, 2, 4)
        assert v.bump_minor() == Version(1, 3)
        assert v.bump_major() == Version(2)
        assert Version(0, 1, 0).bump_major() == Version(1)
        assert Version(0, 1).bump_patch() == Version(0, 1, 1)

    def test_str(self):
        assert str(Version(1)) == "1.0.0"
        assert str(Version(0, 1)) == "0.1.0"
        assert str(Version(1, 2, 3)) == "1.2.3"
        assert str(Version(1, 0, 10)) == "1.0.10"

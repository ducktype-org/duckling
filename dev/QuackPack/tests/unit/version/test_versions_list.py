from quackpack.config.project import VersionList
from quackpack.util.version import Version


def test_single_entry():
    result = VersionList.from_raw_data("2.3.4")
    assert len(result) == 1
    assert isinstance(result[0], Version)
    assert result[0] == Version(2, 3, 4)


def test_list_entry():
    result = VersionList.from_raw_data(["0.1", "1.2.3"])
    assert len(result) == 2
    assert isinstance(result[0], Version)
    assert isinstance(result[1], Version)
    assert result[0] == Version(0, 1)
    assert result[1] == Version(1, 2, 3)


def test_ored_entry():
    result = VersionList.from_raw_data("0.1 or 1.2.3")
    assert len(result) == 2
    assert isinstance(result[0], Version)
    assert isinstance(result[1], Version)
    assert result[0] == Version(0, 1)
    assert result[1] == Version(1, 2, 3)


def test_iter():
    version_list = VersionList.from_raw_data("1.2.3 or 1.2.3")
    for x in version_list:
        assert x == Version(1, 2, 3)


def test_reverse():
    version_list = VersionList.from_raw_data("1.2.3 or 2.3.4")
    reversed_iterator = reversed(version_list)
    assert next(reversed_iterator) == Version(2, 3, 4)
    assert next(reversed_iterator) == Version(1, 2, 3)


def test_contains():
    version_list = VersionList.from_raw_data("1.2.3 or 2.3.4")
    assert Version(1, 2, 3) in version_list
    assert Version(2, 3, 4) in version_list
    assert Version(3, 4, 5) not in version_list


def test_index():
    version_list = VersionList.from_raw_data("1.2.3 or 2.3.4")
    assert version_list.index(Version(2, 3, 4)) == 1


def test_count():
    version_list = VersionList.from_raw_data("1.2.3 or 2.3.4 or 1.2.3")
    assert version_list.count(Version(1, 2, 3)) == 2


def test_parse_already_version():
    version_list = VersionList.from_raw_data([Version(1, 2, 3)])
    assert len(version_list) == 1
    assert version_list[0] == Version(1, 2, 3)

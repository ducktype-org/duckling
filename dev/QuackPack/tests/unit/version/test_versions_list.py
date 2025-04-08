from quackpack.config.project.models import VersionList
from quackpack.util.version import Version


class TestVersionsList:
    def test_single_entry(self):
        result = VersionList.from_raw_data("2.3.4")
        assert len(result) == 1
        assert isinstance(result[0], Version)
        assert result[0] == Version(2, 3, 4)

    def test_list_entry(self):
        result = VersionList.from_raw_data(["0.1", "1.2.3"])
        assert len(result) == 2
        assert isinstance(result[0], Version)
        assert isinstance(result[1], Version)
        assert result[0] == Version(0, 1)
        assert result[1] == Version(1, 2, 3)

    def test_or_entry(self):
        result = VersionList.from_raw_data("0.1 or 1.2.3")
        assert len(result) == 2
        assert isinstance(result[0], Version)
        assert isinstance(result[1], Version)
        assert result[0] == Version(0, 1)
        assert result[1] == Version(1, 2, 3)

    def test_iter(self):
        version_list = VersionList.from_raw_data("1.2.3 or 1.2.3")
        for x in version_list:
            assert x == Version(1, 2, 3)

    def test_reverse(self):
        version_list = VersionList.from_raw_data("1.2.3 or 2.3.4")
        reversed_iterator = reversed(version_list)
        assert next(reversed_iterator) == Version(2, 3, 4)
        assert next(reversed_iterator) == Version(1, 2, 3)

    def test_contains(self):
        version_list = VersionList.from_raw_data("1.2.3 or 2.3.4")
        assert Version(1, 2, 3) in version_list
        assert Version(2, 3, 4) in version_list
        assert Version(3, 4, 5) not in version_list

    def test_index(self):
        version_list = VersionList.from_raw_data("1.2.3 or 2.3.4")
        assert version_list.index(Version(2, 3, 4)) == 1

    def test_count(self):
        version_list = VersionList.from_raw_data("1.2.3 or 2.3.4 or 1.2.3")
        assert version_list.count(Version(1, 2, 3)) == 2

    def test_parse_already_version(self):
        version_list = VersionList.from_raw_data([Version(1, 2, 3)])
        assert len(version_list) == 1
        assert version_list[0] == Version(1, 2, 3)

from common.version import Version, version_list_parser


class TestVersionsList:
    def test_single_entry(self):
        result = version_list_parser("2.3.4")
        assert isinstance(result, list)
        assert len(result) == 1  # type: ignore[attr-defined]
        assert isinstance(result[0], Version)
        assert result[0].major == 2
        assert result[0].minor == 3
        assert result[0].patch == 4

    def test_list_entry(self):
        result = version_list_parser(["0.1", "1.2.3"])
        assert isinstance(result, list)
        assert len(result) == 2  # type: ignore[attr-defined]
        assert isinstance(result[0], Version)
        assert isinstance(result[1], Version)
        assert result[0].major == 0
        assert result[0].minor == 1
        assert result[0].patch == 0
        assert result[1].major == 1
        assert result[1].minor == 2
        assert result[1].patch == 3

    def test_or_entry(self):
        result = version_list_parser("0.1 or 1.2.3")
        assert isinstance(result, list)
        assert len(result) == 2  # type: ignore[attr-defined]
        assert isinstance(result[0], Version)
        assert isinstance(result[1], Version)
        assert result[0].major == 0
        assert result[0].minor == 1
        assert result[0].patch == 0
        assert result[1].major == 1
        assert result[1].minor == 2
        assert result[1].patch == 3

from quackpack.util.levenshtein import distance


class TestLevenshtein:
    def test_empty(self):
        assert distance("", "") == 0

    def test_easy(self):
        assert distance("a", "a") == 0
        assert distance("", "xd") == 2
        assert distance("xd", "") == 2
        assert distance("ABCD", "AF") == 3
        assert distance("ABCD", "ABF") == 2
        assert distance("KOTA", "KOTA") == 0

    def test_wikipedia(self):
        assert distance("kitten", "sitting") == 3
        assert distance("uninformed", "uniformed") == 1

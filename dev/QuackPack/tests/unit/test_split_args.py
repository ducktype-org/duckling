from quackpack.setup_and_run import split_args_for_script


class TestSplit:
    def test_split(self):
        assert split_args_for_script(["a", "b", "c"]) == (["a", "b", "c"], None)
        assert split_args_for_script(["--", "a", "b"]) == ([], ["a", "b"])
        assert split_args_for_script(["build", "--", "xd", "--"]) == (["build"], ["xd", "--"])
        assert split_args_for_script([]) == ([], None)

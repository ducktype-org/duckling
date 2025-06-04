from quackpack.scripts import split_args_for_script


def test_split():
    assert split_args_for_script(["a", "b", "c"]) == (["a", "b", "c"], None)
    assert split_args_for_script(["--", "a", "b"]) == ([], ["a", "b"])
    assert split_args_for_script(["build", "--", "xd", "--"]) == (["build"], ["xd", "--"])
    assert split_args_for_script([]) == ([], None)

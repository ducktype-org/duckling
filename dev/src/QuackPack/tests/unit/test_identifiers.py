from quackpack.util.duckling_compatibility import is_valid_identifier


def test_valid_ident():
    assert is_valid_identifier("a")
    assert is_valid_identifier("A")
    assert is_valid_identifier("_")  # TODO na pewno?
    assert is_valid_identifier("_a")
    assert is_valid_identifier("_aA09")
    assert is_valid_identifier("a_bc")
    assert is_valid_identifier("duckling")
    assert is_valid_identifier("duckling2")


def test_invalid_ident():
    assert not is_valid_identifier("-")
    assert not is_valid_identifier("(")
    assert not is_valid_identifier(")")
    assert not is_valid_identifier("{")
    assert not is_valid_identifier("}")
    assert not is_valid_identifier("[")
    assert not is_valid_identifier("]")
    assert not is_valid_identifier("ą")
    assert not is_valid_identifier(" ")
    assert not is_valid_identifier(",")
    assert not is_valid_identifier(".")
    assert not is_valid_identifier("/")
    assert not is_valid_identifier("@a")
    assert not is_valid_identifier("7")
    assert not is_valid_identifier("0xdeadbeef")
    assert not is_valid_identifier("")

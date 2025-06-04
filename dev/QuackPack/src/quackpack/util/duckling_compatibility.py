import re

_pattern = re.compile(r"[a-zA-Z_][a-zA-Z0-9_]*")


# TODO jeszcze nie ustalone, czy np nie chcemy mieć takich samych ograniczeń
# jak na nazwy zmiennych w ducklingu (czyli jakiś ogólniejszy unicode)
# TODO (Stach): pewnie słowa kluczowe Ducklinga też trzeba wykluczyć
def is_valid_identifier(name: str) -> bool:
    """
    Check, whether `name` is valid Duckling identifier.
    """
    # FIXME: Disallow Duckling keywords.
    return _pattern.fullmatch(name) is not None

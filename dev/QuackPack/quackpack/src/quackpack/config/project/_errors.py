# TODO: Should we provide extra data to exceptions, like package/alias name, or version?
class LtComparisonWithNonzeroMinorOrPatchError(Exception):
    def __init__(self):
        super().__init__(
            "The < operator on the right hand side can take versions of form <major>.0.0 or 0.<minor>.0"
        )


class BadTokenError(Exception):
    def __init__(self, token: str):
        # TODO: Use fstrings?
        super().__init__(
            rf"Error while parsing the token: {token}. Such a token should satisfy the regex: (~=|>=|>|<)(\[[a-z_0-9]*(,[a-z_0-9]*)*\])?[0-9]*(.[0-9]*)?(.[0-9]*)?"
        )


class DependencyAlreadyExistsError(Exception):
    def __init__(self):
        super().__init__(
            "You already have this dependecy specified. If You would like to override use flag -f"
        )


class RemoveOrChangeNonexistentDependencyError(Exception):
    def __init__(self):
        super().__init__("There is no such dependency")


class AliasAlreadyExistsError(Exception):
    def __init__(self):
        super().__init__(
            "You already have alias for this package specified. If You would like to override use flag -f"
        )


class RemoveNonexistentAliasError(Exception):
    def __init__(self):
        super().__init__("There is no such alias")


class AmbivalentAliasError(Exception):
    def __init__(self):
        super().__init__("You cannot give the same alias to two different packages")

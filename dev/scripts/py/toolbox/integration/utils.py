from typing import Optional

from ..helpers import exit_with_error, log_warning


def make_test_name(name: str) -> str:
    # for c in name:
    #     if c == c.upper():
    #         exit_with_error(
    #             f"Name should be in snake_case or kebab-case, but not: {name}"
    #         )

    name = name.replace("tests", "")
    name = name.replace("Tests", "")
    name = name.replace("test", "")
    name = name.replace("Test", "")

    name = name.capitalize()
    name = name.replace("-", "_")
    name = name.replace(" ", "_")
    while "__" in name:
        name = name.replace("__", "_")
    i = name.find("_")
    while i != -1:
        name = name[:i] + " " + name[i + 1 :].capitalize()
        i = name.find("_")

    return name.lstrip().rstrip()


def resembles_builtin(name: str, builtin_set: set[str]) -> Optional[str]:
    for key in builtin_set:
        if name.lower() == key.lower() and name != key:
            log_warning(
                f"Incorrect spelling of '{name}' in config file. Consider: '{key}'"
            )
            return key
    return None

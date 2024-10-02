

from dev.scripts.toolbox.helpers import bash_command


def simulate_clang_tidy(path: str):
    diff = bash_command_get_ouput("git diff ")





from scripts.toolbox.helpers import bash_command_get_output


def simulate_clang_tidy(path: str):
    diff = bash_command_get_output("git diff --merge-base main")
    print(diff[0].decode('UTF-8'))

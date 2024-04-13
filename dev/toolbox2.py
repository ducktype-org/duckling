#!/usr/bin/python3

import sys


commands = []
help_messages = {}


def make_action(help):
    def make_action_wrapper(func):
        global commands, help_messages
        func_name = func.__name__
        commands.append(func_name)
        help_messages[func_name] = help

        return func

    return make_action_wrapper


@make_action("Runs stuff")
def lolz(args):
    print("Lolz is running!", args)


@make_action("Setups an virtual environment")
def make_venv(args):
    print("Making venv...", args)


@make_action("Writes a help message")
def help(args):
    print("This is a toolbox, a helper program to maker life easier.\n")

    print("Available actions: ")
    for cmd in help_messages:
        print(f"\t {cmd} \t {help_messages[cmd]}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        help(sys.argv)
    else:
        cmd = sys.argv[1]
        if cmd not in commands:
            print(f'Invalid command: \'{cmd}\'!')
            print(f'Run "./toolbox.py help" for more information.')
            exit(1)

        exec(f"{sys.argv[1]}(sys.argv[2:])")

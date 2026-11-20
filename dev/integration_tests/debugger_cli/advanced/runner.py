# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import subprocess
import sys
import os
import threading
import typing

whole_output = ''


def fail(reason: str) -> typing.NoReturn:
    print(f"TEST FAILED: {reason}", flush=True)
    try:
        vm_process.kill()
    except: pass
    finally:
        sys.exit(1)

def succeed() -> typing.NoReturn:
    try:
        vm_process.terminate()
    except: pass
    finally:
        sys.exit(0)


def expect(expected: str, timeout: float) -> None | typing.NoReturn:
    global whole_output
    timeout_timer = threading.Timer(timeout, vm_process.kill)
    timeout_timer.start()

    output = ''
    while c := vm_process.stdout.read(1):
        output += c
        if output.endswith(expected):
            timeout_timer.cancel()
            whole_output += output
            return True
    else:
        sys.stderr.write(f'Expected: {repr(expected)}')
        sys.stderr.write(f'Got: {repr(output)}')
        sys.stderr.write(f'Whole execution: \n{whole_output}{output}')
        fail("expected test did not show before timeout")


def write(text: str) -> None:
    global whole_output
    whole_output += f'<wrote {repr(text)}>'
    vm_process.stdin.write(text)
    vm_process.stdin.flush()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.stderr.write("ERROR: Build directory path was not provided as an argument!\n")
        sys.exit(1)

    line = input()
    debugger_args = line.split(' ') if line else []

    build_dir = sys.argv[1]
    vm_binary_path = os.path.join(build_dir, "bin", "VM")

    vm_process_env = os.environ.copy()
    vm_process_env['DUCK_VM_UNBUFFERED'] = '1'

    vm_process = subprocess.Popen(
        [vm_binary_path, "run", "-d", *debugger_args],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        env=vm_process_env,
    )


    timeout = 1
    try:
        while True:
            line = input()

            if any([
                line.strip() == "",
                line.startswith("#"),
            ]): continue

            cmd, arg = line.split(' ', maxsplit=1)
            arg = eval(arg, dict())

            match cmd:
                case "<":
                    write(arg)
                case ">":
                    expect(arg, timeout)
                case "timeout":
                    timeout = arg
                case _:
                    fail(f"incorrect line: {repr(line)}")
    except EOFError:
        succeed()

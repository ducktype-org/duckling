import subprocess
import sys
import os
import threading
import time

whole_output = ''


def timeout_timer(timeout, stop_event):
    start_time = time.time()
    
    while time.time() - start_time < timeout and not stop_event.is_set():
        time.sleep(0.001)

    if stop_event.is_set():
        return
    
    print("[timeout_timer]: Timeout!!! Killing the VM", file=sys.stderr)
    vm_process.kill()


def expect(expected: str, timeout = 0.1):
    global whole_output
    success = threading.Event()
    timeout_timer_thread = threading.Thread(
        target=timeout_timer,
        args=(timeout, success),
        daemon=True
    )
    timeout_timer_thread.start()
    
    output = ''
    while c := vm_process.stdout.read(1):
        output += c
        if output.endswith(expected):
            success.set()
            whole_output += output
            timeout_timer_thread.join()
            return True
    else:
        print(f'Expected: {repr(expected)}', file=sys.stderr)
        print(f'Got: {repr(output)}', file=sys.stderr)
        print(f'Whole execution looked as follows: \n{whole_output}{output}', file=sys.stderr)
        print("__TEST_FAIL__", flush=True)
        exit(1)


def write(text: str):
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
    vm_process_env['UNBUFFERED'] = '1'

    vm_process = subprocess.Popen(
        [vm_binary_path, "run", "-d", *debugger_args],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        env=vm_process_env,
    )

    def interpret(line: str):
        return line[2:].replace('\\n', '\n')

    while line := input():
        if line.startswith("<"):
            write(interpret(line) + '\n')
        elif line.startswith(">"):
            expect(interpret(line))
        elif line.startswith("DONE"):
            print("__TEST_SUCC__", flush=True)
            vm_process.terminate()
            exit(0)

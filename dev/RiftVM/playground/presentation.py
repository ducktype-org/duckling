import os
from termcolor import colored

wait_time = 5
pid = 0

get = colored("GET", "light_blue")
delete = colored("DELETE", "light_blue")
post = colored("POST", "light_blue")
put = colored("PUT", "light_blue")


def status(id):
    print(get + " /status/{}:".format(id))
    os.system("curl -X 'GET' \
              'http://127.0.0.1:5000/status/{}' \
              -H 'accept: application/json'".format(id))
    print()
    input()


def spawn():
    print(put + " /process/spawn:")
    os.system("curl -X 'PUT' \
              'http://127.0.0.1:5000/process/spawn' \
              -H 'accept: application/json'")
    print()
    input()


def kill(id):
    print(delete + " /process/kill/{}:".format(id))
    os.system("curl -X 'DELETE' \
              'http://127.0.0.1:5000/process/kill/{}' \
              -H 'accept: application/json'".format(id))
    print()
    input()


def load(id, path):
    print(post + " /process/load/{}:".format(id))
    os.system("curl -X 'POST' \
              'http://127.0.0.1:5000/process/load/{}' \
              -H 'accept: application/json' \
              -H 'Content-Type: text/plain' \
              -d '{}'".format(id, path))
    print()
    input()


def run(id):
    print(post + " /process/run/{}:".format(id))
    os.system("curl -X 'POST' \
              'http://127.0.0.1:5000/process/run/{}' \
              -H 'accept: application/json'".format(id))
    print()
    input()


def stop(id):
    print(post + " /process/stop/{}:".format(id))
    os.system("curl -X 'POST' \
              'http://127.0.0.1:5000/process/stop/{}' \
              -H 'accept: application/json'".format(id))
    print()
    input()


def pause(id):
    print(post + " /debug/pause/{}:".format(id))
    os.system("curl -X 'POST' \
              'http://127.0.0.1:5000/debug/pause/{}' \
              -H 'accept: application/json'".format(id))
    print()
    input()


def step(id, count):
    print("{} x " + post + " /debug/step/{}:".format(count, id))
    for _ in range(count):
        os.system("curl -X 'POST' \
                'http://127.0.0.1:5000/debug/step/{}' \
                -H 'accept: application/json'".format(id))
        print()
        # input()
    input()


def resume(id):
    print(post + " /debug/resume/{}:".format(id))
    os.system("curl -X 'POST' \
              'http://127.0.0.1:5000/debug/resume/{}' \
              -H 'accept: application/json'".format(id))
    print()
    input()


def vm_input(id, vm_input):
    print(post + " /process/input/{}\nBody: {}".format(id, vm_input))
    os.system("curl -X 'POST' \
              'http://127.0.0.1:5000/process/input/{}' \
              -H 'accept: application/json' \
              -H 'Content-Type: text/plain' \
              -d '{}'".format(id, vm_input))
    print()
    input()


def output(id):
    print(get + " /process/output/{}:".format(id))
    os.system("curl -X 'GET' \
              'http://127.0.0.1:5000/process/output/{}' \
              -H 'accept: application/json'".format(id))
    print()
    input()


def type(id, name):
    print(get + " /data/type/{}/{}:".format(id, name))
    os.system("curl -X 'GET' \
              'http://127.0.0.1:5000/data/type/{}/{}' \
              -H 'accept: application/json'".format(id, name))
    print()
    input()


print(colored("Spawning VCPU:", "light_green"))
spawn()

print(colored("Loading RiftBC:", "light_green"))
load(pid, '/home/andrzej/mine/rift/rift-poc-zpp1/dev/RiftVM/snippets_f8/working.rbc')

print(colored("Running program:", "light_green"))
run(pid)
vm_input(pid, '10')
output(pid)

print(colored("Stoping program:", "light_green"))
stop(pid)

type(pid, "int64")

print(colored("Rerunning program:", "light_green"))
run(pid)
status(pid)

print(colored("Pausing:", "light_green"))
pause(pid)
vm_input(pid, '10')

print(colored("Step by step execution:", "light_green"))
step(pid, 1)
status(pid)
step(pid, 15)
output(pid)

print(colored("Resuming program:", "light_green"))
resume(pid)
output(pid)

print(colored("Killing VCPU:", "light_green"))
kill(pid)

status(0)

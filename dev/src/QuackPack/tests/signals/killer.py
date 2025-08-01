import sys
from os import kill
from signal import SIGINT, SIGTERM
from time import sleep


def handle_signal_context(peer_pid: int):
    sleep(0.5)
    kill(peer_pid, SIGINT)
    sleep(0.5)
    kill(peer_pid, SIGINT)


def handle_robust_context_consume(peer_pid: int):
    sleep(0.1)
    kill(peer_pid, SIGINT)
    sleep(0.2)
    kill(peer_pid, SIGTERM)
    kill(peer_pid, SIGTERM)
    sleep(0.2)
    kill(peer_pid, SIGINT)


def handle_robust_context_interrupt(peer_pid: int):
    sleep(0.3)
    kill(peer_pid, SIGINT)
    sleep(0.3)
    kill(peer_pid, SIGTERM)
    sleep(0.3)
    kill(peer_pid, SIGINT)
    sleep(0.5)
    kill(peer_pid, SIGTERM)
    sleep(0.5)
    kill(peer_pid, SIGINT)


def handle_robust_context_no_interrupt(peer_pid: int):
    sleep(0.8)
    kill(peer_pid, SIGINT)
    kill(peer_pid, SIGTERM)


def handle_robust_context_force_kill(peer_pid: int):
    sleep(0.3)
    kill(peer_pid, SIGINT)
    sleep(2)
    kill(peer_pid, SIGTERM)


def handle_robust_context_no_force_kill(peer_pid: int):
    sleep(0.1)
    for _ in range(100):
        kill(peer_pid, SIGINT)
    sleep(0.6)
    kill(peer_pid, SIGTERM)


if __name__ == "__main__":
    peer_pid = int(sys.argv[1])
    test_case = sys.argv[2]
    if test_case == "signal_context":
        handle_signal_context(peer_pid)
    elif test_case == "robust_context_consume":
        handle_robust_context_consume(peer_pid)
    elif test_case == "robust_context_interrupt":
        handle_robust_context_interrupt(peer_pid)
    elif test_case == "robust_context_no_interrupt":
        handle_robust_context_no_interrupt(peer_pid)
    elif test_case == "robust_context_force_kill":
        handle_robust_context_force_kill(peer_pid)

import sys
from os import kill
from signal import SIGINT, SIGTERM
from time import sleep

if __name__ == "__main__":
    peer_pid = int(sys.argv[1])
    sleep(0.6)
    kill(peer_pid, SIGINT)
    sleep(0.3)
    kill(peer_pid, SIGTERM)

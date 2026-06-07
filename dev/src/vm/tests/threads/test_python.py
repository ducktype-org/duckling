import threading
import sys

def inc(iterations):
    counter = 0
    i = iterations

    while i > 0:
        i -= 1
        counter += 1
    print(counter, end="")

def main():
    if len(sys.argv) < 2:
        print("Użycie: python program.py <liczba_wątków>")
        return

    n = int(sys.argv[1])
    threads = []

    iterations_per_thread = 10000000 // n

    # Tworzenie wątków
    for _ in range(n):
        t = threading.Thread(target=inc, args=(iterations_per_thread,))
        threads.append(t)
        t.start()

    # Join
    for t in threads:
        t.join()

if __name__ == "__main__":
    main()
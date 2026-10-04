import sys

a = 0
b = 1

for _ in range(int(sys.argv[1])):
    print(a % int(1e9 + 7), end=" ")
    b += a
    a = b - a

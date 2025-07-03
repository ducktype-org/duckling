def collatz(n):
    j = 0
    while n != 1:
        if n % 2 == 0:
            n //= 2
        else:
            n = n * 3 + 1
        j = max(j, n)
    return j


x = int(input())
s = 0
for i in range(1, x):
    s = max(s, collatz(i))
print(s)

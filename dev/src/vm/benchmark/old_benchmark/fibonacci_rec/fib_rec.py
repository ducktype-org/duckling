def fib(n):
    if n <= 1:
        return n
    return (fib(n - 1) + fib(n - 2)) % m


x = int(input())
m = 8388449

print(fib(x))

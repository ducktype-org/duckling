x = int(input())
m = int(input())

a = 0
b = 1
for i in range(x):
    c = (a + b) % m
    a = b
    b = c

print(a)

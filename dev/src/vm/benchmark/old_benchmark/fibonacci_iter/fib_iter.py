x, m = input().rstrip().split(' ')
x = int(x)
m = int(m)

a = 0
b = 1
for i in range(x):
    c = (a + b) % m
    a = b
    b = c

print(a)

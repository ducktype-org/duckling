# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

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

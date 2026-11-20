# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

def fib(n):
    if n <= 1:
        return n
    return (fib(n - 1) + fib(n - 2)) % m


x = int(input())
m = 8388449

print(fib(x))

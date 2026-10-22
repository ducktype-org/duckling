# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import sys

a = 0
b = 1

for _ in range(int(sys.argv[1])):
    print(a % int(1e9 + 7), end=" ")
    b += a
    a = b - a

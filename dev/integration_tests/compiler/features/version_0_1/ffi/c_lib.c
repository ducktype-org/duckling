// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include<stdio.h>
#include <stdint.h>

int64_t say_hello(int64_t times) {
    for (int64_t i = 0; i < times; i++) {
        printf("Hello from C!\n");
    }
    return 0;
}

// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file opcodes_functions_impl_exec.cpp
 * @author Wojciech Rzepliński
 * @brief The opcodes functions implementations.
 * Use this include and do not include `opcodes_functions_impl_base.def.hpp` directly,
 * because there are two versions of the opcodes functions: regular and debug.
 * This includes the regular version, the one that is used in main thread execution loop.
 */

#include "opcodes_functions_impl_base.def.hpp"

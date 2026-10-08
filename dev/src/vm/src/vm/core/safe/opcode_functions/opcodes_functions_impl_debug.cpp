// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file opcodes_functions_impl_debug.cpp
 * @author Wojciech Rzepliński
 * @brief The opcodes functions implementations in debug mode.
 * This file includes the debug version of the opcodes.
 * They are used in "step-by-step" execution mode.
 */

#define DEBUG_OPCODES
#include "opcodes_functions_impl_base.def.hpp"
#undef DEBUG_OPCODES

/**
 * @file opcodes_debug.hpp
 * @author Wojciech Rzepliński
 * @brief The opcodes functions implementations in debug mode.
 * This file includes the debug version of the opcodes.
 * They are used in "step-by-step" execution mode.
 */
#pragma once

#define DEBUG_OPCODES
#include "opcodes_functions_implementation.hpp"
#undef DEBUG_OPCODES

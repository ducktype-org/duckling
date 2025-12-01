#pragma once

#include <base/preproc/utils.hpp>

/**
 * @brief Removes parentheses from around arguments.
 *
 * Example:
 * REMOVE_PARENTHESES((A, B, C)) expands to A, B, C
 *
 * This is useful when dealing with macros parameters that may contain commas.
 * Parentheses "safeguard" commas in macro arguments, while this macro removes them.
 */
#define REMOVE_PARENTHESES(args) EXPAND(EXPAND_VA_ARGS args)

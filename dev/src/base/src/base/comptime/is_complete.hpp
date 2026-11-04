// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <type_traits>

template<typename, typename = void>
struct IS_COMPLETE {
	static constexpr bool VALUE = false;
};

template<typename T>
struct IS_COMPLETE<T, std::void_t<decltype(sizeof(T))>> {
	static constexpr bool VALUE = true;
};

/**
 * @brief Check if a type is complete.
 *
 * Usage: IS_COMPLETE_V<T>
 *
 * Note: if this template returns true, then the type is complete, and everything is OK.
 * However, this type trait may return a false negative.
 * Use it only to guarantee completeness in critical code fragments.
 *
 * This is because this template may be evaluated before T is complete, and due
 * to the one definition rule, the result will not change, so it will be false
 * even after T becomes complete.
 *
 * @tparam T Type to check for completeness.
 */
template<typename T>
constexpr bool IS_COMPLETE_V = IS_COMPLETE<T>::VALUE;

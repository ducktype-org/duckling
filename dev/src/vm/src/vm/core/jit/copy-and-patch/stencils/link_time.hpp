// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/except/exceptions.hpp>

#include <bit>
#include <cstdint>
#include <functional>

namespace vm::jit::cnp::internal {
	struct OpaqueStruct;

	template<class Type, class IntegerEquivalent>
	Type valueFromPointer(OpaqueStruct& link_time_variable) {
		auto value = std::bit_cast<std::uintptr_t>(&link_time_variable);

		static_assert(
			sizeof(IntegerEquivalent) <= sizeof(std::uintptr_t),
			"Type too big for a link-time constant"
		);
		auto truncated_value = static_cast<IntegerEquivalent>(value);

		static_assert(sizeof(IntegerEquivalent) == sizeof(Type), "Wrong size of link-time constant");
		return std::bit_cast<Type>(truncated_value);
	}
}

#define LINK_VARIABLE_NAME(name) _value_to_patch_##name

// Those variables are also implicitly extern:
// https://en.cppreference.com/cpp/language/language_linkage#Notes
#define DECLARE_LINK_VARIABLE(name) \
	__attribute__((weak)) extern "C" internal::OpaqueStruct LINK_VARIABLE_NAME(name)

#define GET_LINK_VARIABLE(name, type, size) \
	internal::valueFromPointer<type, u##size>(LINK_VARIABLE_NAME(name))

#define LINK_VALUE(name, type, size)                \
	std::invoke([] {                                \
		DECLARE_LINK_VARIABLE(name);                \
		return GET_LINK_VARIABLE(name, type, size); \
	})

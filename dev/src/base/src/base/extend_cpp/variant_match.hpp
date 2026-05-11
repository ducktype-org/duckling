/**
 * @file variant.hpp
 *
 * @brief Implements additional `std::variant` functionalities.
 *
 * Functionalities
 * ===============
 *
 * Variant visit
 * -------------
 *
 * Variant visit provides macros that simplify `std::visit` calls.
 *
 * - `VARIANT_VISIT`
 * - `VISIT_CASE`
 * - `VISIT`
 *
 * ### Usage
 *
 * @include variant_visit_example.cpp
 *
 * Variant match
 * -------------
 *
 * @note This functionality is macro-based. Braces are very important for it to work properly.
 *
 * Variant match is a macro that allows matching over `std::variant` types.
 *
 * - `variant_match`
 * - `variant_case`
 * - `variant_case_novalue`
 * - `variant_default`
 *
 * ### Usage
 *
 * @include variant_match_example.cpp
 *
 * @example variant_visit_example.cpp
 *
 * @example variant_match_example.cpp
 */
#pragma once

#include <base/comptime/type_traits.hpp>
#include <base/preproc/diagnostics.hpp>
#include <base/types/ints.hpp>

#include <type_traits>
#include <variant>

namespace base::internal {
	template<typename... T>
	struct VisitOverloaded final: T... {
		using T::operator()...;
	};

	template<class... Ts>
	VisitOverloaded(Ts...) -> VisitOverloaded<Ts...>;
}

namespace base {
	/**
	 * @brief Helper function for filtering variants
	 */
	template<typename T, typename U>
	static bool holds(const U& el) {
		return std::holds_alternative<T>(el);
	}

	/**
	 * @brief Helper function for extracting from variants
	 */
	template<typename T, typename U>
	static T choose(const U& el) {
		return std::get<T>(el);
	}
}

/**
 * @brief Use instead of `holds_alternative` if-chains
 * implementation is temporary
 *
 * Usage:
 * variant_match(variant_variable) {
 * 	variant_case(variant_option_type, variable_name) {
 * 		code using variable name as variant_option_type type;
 * 	}
 * 	variant_case_novalue(variant_option_type) {
 * 		code;
 * 	}
 * 	variant_default {
 * 		code;
 * 	}
 * }
 *
 * Braces are IMPORTANT for the code to work properly
 */
#define variant_match(value) \
	PUSH_DIAGNOSTIC          \
	NO_SHADOW switch (auto&& internal_value = (value); internal_value.index()) POP_DIAGNOSTIC

#define variant_case(type, name)                                       \
	PUSH_DIAGNOSTIC NO_SHADOW break;                                   \
	case (::base::variantTypeIndex<decltype(internal_value), type>()): \
		if ([[maybe_unused]] auto&& name = std::get<type>(internal_value); true) POP_DIAGNOSTIC

#define variant_case_novalue(type)                                     \
	break;                                                             \
	case (::base::variantTypeIndex<decltype(internal_value), type>()): \
		if (true)

#define variant_default \
	break;              \
	default:            \
		if (true)

/**
 * @brief Use instead of `std::visit` with multiple choices.
 */
#define VARIANT_VISIT(value, /*cases*/...) \
	{ std::visit(::base::internal::VisitOverloaded{ __VA_ARGS__ }, (value)); }

#define VISIT_CASE(type, name, code) [&](type name) { code; }


/**
 * @brief Use instead of simple `std::visit`.
 */
#define VISIT(variant_value, name, code) std::visit([&](auto&& name) { code; }, (variant_value))

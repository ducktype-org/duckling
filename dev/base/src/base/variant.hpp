#pragma once

#include "define_helper.hpp"

#include <type_traits>
#include <variant>

// See usage in test

namespace base {
	template<typename... T>
	struct VisitOverloaded: T... {
		using T::operator()...;
	};

	template<class... Ts>
	VisitOverloaded(Ts...) -> VisitOverloaded<Ts...>;

	template<typename VariantT, typename T, std::size_t index>
	constexpr auto alternative_index_aux() {
		static_assert(std::variant_size_v<VariantT> > index, "Type not found in variant");
		if constexpr (index == std::variant_size_v<VariantT>) {
			return index;
		} else if constexpr (std::is_same_v<std::variant_alternative_t<index, VariantT>, T>) {
			return index;
		} else {
			return alternative_index_aux<VariantT, T, index + 1>();
		}
	}

	template<typename VariantT, typename T>
	constexpr auto alternative_index() {
		return alternative_index_aux<std::remove_const_t<std::remove_reference_t<VariantT>>,
		                             T,
		                             0>();
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
 * 	variant_case(variant_option_type) {
 * 		code;
 * 	}
 * 	variant_default {
 * 		code;
 * 	}
 * }
 *
 * Braces are IMPORTANT for the code to work properly
 */
#define variant_match(value)                                                                       \
	PUSH_DIAGNOSTIC NO_SHADOW if (bool variant_match_stop                                          \
	                              = true) for (auto& internal_value = (value); variant_match_stop; \
	                                           variant_match_stop                                  \
	                                           = false) switch (internal_value.index())            \
		POP_DIAGNOSTIC

#define variant_case(type, name)                                                                  \
	PUSH_DIAGNOSTIC NO_SHADOW break;                                                              \
	case (base::alternative_index<decltype(internal_value), type>()):                             \
		if (bool variant_case_stop = true)                                                        \
			for ([[maybe_unused]] auto& name = std::get<type>(internal_value); variant_case_stop; \
			     variant_case_stop           = false)                                             \
		POP_DIAGNOSTIC

#define variant_case_novalue(type)                                    \
	break;                                                            \
	case (base::alternative_index<decltype(internal_value), type>()): \
		if (true)

#define variant_default \
	break;              \
	default:            \
		if (true)

/**
 * @brief Use instead of `std::visit` with multiple choices
 */
#define VARIANT_VISIT(value, code) \
	{ std::visit(::base::VisitOverloaded{ code }, (value)); }

#define VISIT_CASE(type, name, code)     [&](type name) { code; },


/**
 * @brief Use instead of simple `std::visit`
 */
#define VISIT(variant_value, name, code) std::visit([&](auto&& name) { code; }, (variant_value))

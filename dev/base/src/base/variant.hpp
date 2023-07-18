#pragma once

#include <variant>

// See usage in test

namespace base {
	template<typename... T>
	struct VisitOverloaded: T... { using T::operator()...; };
	
	template<class... Ts>
	VisitOverloaded(Ts...) -> VisitOverloaded<Ts...>;
}

/**
 * @brief Use instead of `holds_alternative` if-chains
 */
#define VARIANT_MATCH(value, code) \
	do{                                      \
		auto& internal_value = (value);      \
		if (false) {}					     \
		code                                 \
	} while(false);


#define VARIANT_CASE(type, name, code) \
	else if (std::holds_alternative<type>(internal_value)) { \
		auto& name = std::get<type>(internal_value); \
		{                                            \
			code                                     \
		}                                            \
	}


#define VARIANT_CASE_NOVALUE(type, code) \
	else if (std::holds_alternative<type>(internal_value)) { \
		{                                            \
			code                                     \
		}                                            \
	}

/**
 * @brief VARIANT_DEFAULT_CASE must be the last case in chain
 */
#define VARIANT_DEFAULT_CASE(code) \
	else { \
		{                                            \
			code                                     \
		}                                            \
	}

/**
 * @brief Use instead of `std::visit` with multiple choices
 */
#define VARIANT_VISIT(value, code) \
	{                                           \
		std::visit(::base::VisitOverloaded {   \
			code                                \
		}, (value)); \
	}

#define VISIT_CASE(type, name, code) \
	[&](type name) { code },


// @TODO: better names
#define VISIT(variant_value, code) std::visit( [&] (auto& value) { code ; }, (variant_value))

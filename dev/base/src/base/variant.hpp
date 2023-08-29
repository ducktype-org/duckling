#pragma once

#include <variant>
#include <type_traits>

// See usage in test

namespace base {
	template<typename... T>
	struct VisitOverloaded: T... { using T::operator()...; };
	
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
		return alternative_index_aux<
			std::remove_const_t<std::remove_reference_t<VariantT> >,
			T,
			0
		>();
	}
}

/**
 * @brief Use instead of `holds_alternative` if-chains
 * implementation is temporary
 */
#define variant_match(value) \
	if (bool variant_match_stop = true) \
	for (auto& internal_value = (value); variant_match_stop; variant_match_stop = false) \
	switch (internal_value.index()) case std::size_t(-1):

#define variant_case(type, name) \
	break; \
	case (base::alternative_index<decltype(internal_value), type>()): \
		if (bool variant_case_stop = true) \
		for (auto& name = std::get<type>(internal_value); variant_case_stop; variant_case_stop = false)


#define variant_case_novalue(type) \
	break; \
	case (base::alternative_index<decltype(internal_value), type>()): \
		if (true)
/**
 * @brief Use instead of `holds_alternative` if-chains
 * @deprecated
 */
#define VARIANT_MATCH(value, code) \
	do{                                      \
		auto& internal_value = (value);      \
		if (false) {}					     \
		code                                 \
	} while(false);


// @deprecated
#define VARIANT_CASE(type, name, code) \
	else if (std::holds_alternative<type>(internal_value)) { \
		auto& name = std::get<type>(internal_value); \
		{                                            \
			code                                     \
		}                                            \
	}


// @deprecated
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

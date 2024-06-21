#pragma once

#include <base/constexpr_cat.hpp>
#include <base/ints.hpp>
#include <variant>
#include <vector>
#include <memory>

template<typename T>
struct TypeParseTraits;

namespace JS::impl {
	template<class... Args>
	struct MakeList;

	template<class Arg>
	struct MakeList<Arg> {
		static constexpr auto name = CONSTEXPR_CAT(TypeParseTraits<Arg>::name);
	};

	template<class Arg1, class Arg2, class... Args>
	struct MakeList<Arg1, Arg2, Args...> {
		static constexpr auto name
			= CONSTEXPR_CAT(TypeParseTraits<Arg1>::name, ", ", MakeList<Arg2, Args...>::name);
	};
}

#define REGISTER_PARSE_TYPE(T)                                \
	template<>                                                \
	struct TypeParseTraits<T> {                               \
		static constexpr auto name = CONSTEXPR_CAT(#T, '\0'); \
	};

#define REGISTER_PARSE_TYPE_ALIAS(T, NAME)                \
	template<>                                            \
	struct TypeParseTraits<T> {                           \
		static constexpr auto name = CONSTEXPR_CAT(NAME); \
	};

#define REGISTER_PARSE_TYPE_TEMPLATE_ALIAS(T, NAME)                                             \
	template<class X>                                                                           \
	struct TypeParseTraits<T<X>> {                                                              \
		static constexpr auto name = CONSTEXPR_CAT(NAME, "<", TypeParseTraits<X>::name, ">\0"); \
	};

#define REGISTER_PARSE_TYPE_TEMPLATE_VARIADIC_ALIAS(T, NAME)                      \
	template<class... Args>                                                       \
	struct TypeParseTraits<T<Args...>> {                                          \
		static constexpr auto name                                                \
			= CONSTEXPR_CAT(NAME, "<", JS::impl::MakeList<Args...>::name, ">\0"); \
	};

REGISTER_PARSE_TYPE_ALIAS(std::string, "string");
REGISTER_PARSE_TYPE(double);
REGISTER_PARSE_TYPE(float);
REGISTER_PARSE_TYPE_ALIAS(u8, "u8");
REGISTER_PARSE_TYPE_ALIAS(i16, "i16");
REGISTER_PARSE_TYPE_ALIAS(u16, "u16");
REGISTER_PARSE_TYPE_ALIAS(i32, "i32");
REGISTER_PARSE_TYPE_ALIAS(u32, "u32");
REGISTER_PARSE_TYPE_ALIAS(i64, "i64");
REGISTER_PARSE_TYPE_ALIAS(u64, "u64");
REGISTER_PARSE_TYPE(bool);

REGISTER_PARSE_TYPE_TEMPLATE_ALIAS(std::vector, "vector")
REGISTER_PARSE_TYPE_TEMPLATE_ALIAS(std::unique_ptr, "unique_ptr")
REGISTER_PARSE_TYPE_TEMPLATE_VARIADIC_ALIAS(std::variant, "variant")

template<class T>
struct TypeParseTraits<T[]> {
	static constexpr auto name = CONSTEXPR_CAT("[", TypeParseTraits<T>::name, "]");
};

#pragma once

#include <base/comptime/constexpr_cat.hpp>
#include <base/types/ints.hpp>

#include <array>
#include <memory>
#include <string_view>
#include <variant>
#include <vector>

template<typename T>
struct TypeParseTraits;

namespace js::impl {
	template<class... Args>
	struct MakeList;

	template<class Arg>
	struct MakeList<Arg> {
		static constexpr auto NAME = CONSTEXPR_CAT(TypeParseTraits<Arg>::NAME);
	};

	template<class Arg1, class Arg2, class... Args>
	struct MakeList<Arg1, Arg2, Args...> {
		static constexpr auto NAME
			= CONSTEXPR_CAT(TypeParseTraits<Arg1>::NAME, ", ", MakeList<Arg2, Args...>::NAME);
	};
}

namespace js {
	template<class T>
	[[nodiscard]] constexpr std::string_view typeName() {
		static_assert(
			TypeParseTraits<T>::NAME.back() == '\0',
			"TypeParseTraits<T>::NAME must be \\0 terminated"
		);
		return std::string_view(
			TypeParseTraits<T>::NAME.data(), TypeParseTraits<T>::NAME.size() - 1
		);
	}
}

#define JSON_REGISTER_TYPE(T)                                 \
	template<>                                                \
	struct TypeParseTraits<T> {                               \
		static constexpr std::array NAME = std::to_array(#T); \
	};

#define JSON_REGISTER_TYPE_WITH_NAME(T, CUSTOM_NAME)                   \
	template<>                                                         \
	struct TypeParseTraits<T> {                                        \
		static constexpr std::array NAME = std::to_array(CUSTOM_NAME); \
	};

#define JSON_REGISTER_TEMPLATE_WITH_NAME(T, CUSTOM_NAME)                            \
	template<class X>                                                               \
	struct TypeParseTraits<T<X>> {                                                  \
		static constexpr std::array NAME                                            \
			= CONSTEXPR_CAT(CUSTOM_NAME, "<", TypeParseTraits<X>::NAME, ">", '\0'); \
	};

#define JSON_REGISTER_TEMPLATE_VARIADIC_WITH_NAME(T, CUSTOM_NAME)                            \
	template<class... Args>                                                                  \
	struct TypeParseTraits<T<Args...>> {                                                     \
		static constexpr auto NAME                                                           \
			= CONSTEXPR_CAT(CUSTOM_NAME, "<", js::impl::MakeList<Args...>::NAME, ">", '\0'); \
	};

JSON_REGISTER_TYPE_WITH_NAME(std::string, "string");
JSON_REGISTER_TYPE(double);
JSON_REGISTER_TYPE(float);
JSON_REGISTER_TYPE_WITH_NAME(u8, "u8");
JSON_REGISTER_TYPE_WITH_NAME(i16, "i16");
JSON_REGISTER_TYPE_WITH_NAME(u16, "u16");
JSON_REGISTER_TYPE_WITH_NAME(i32, "i32");
JSON_REGISTER_TYPE_WITH_NAME(u32, "u32");
JSON_REGISTER_TYPE_WITH_NAME(i64, "i64");
JSON_REGISTER_TYPE_WITH_NAME(u64, "u64");
JSON_REGISTER_TYPE(bool);

JSON_REGISTER_TEMPLATE_WITH_NAME(std::vector, "vector")
JSON_REGISTER_TEMPLATE_WITH_NAME(std::unique_ptr, "unique_ptr")
JSON_REGISTER_TEMPLATE_VARIADIC_WITH_NAME(std::variant, "variant")

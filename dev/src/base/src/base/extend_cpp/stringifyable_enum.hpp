// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file stringifyable_enum.hpp
 *
 * @brief This library provides a simple way of creating `enum class` types,
 * that can be automatically converted to `base::StrID` and vice versa.
 *
 * Functionalities
 * ===============
 *
 * MAKE_STRINGIFYABLE_ENUM
 * -----------------------
 *
 * Macro generating enum. It takes three main parameters: namespace, base type,
 * enum name. All other parameters are treated as enum members.
 *
 * @attention For technical reasons `MAKE_STRINGIFYABLE_ENUM` must be used
 * in top-level code only. That is why `namespace` parameter exists. It states
 * in what namespace the enum will be created.
 *
 * Conversion to and from StrID
 * ------------------------------
 *
 * - `base::enumToStr`
 * - `base::strToEnum`
 *
 * ### Usage
 * @include stringifyable_enum_example.cpp
 *
 * @example stringifyable_enum_example.cpp
 */
#pragma once

#include <base/collections/maps.hpp>
#include <base/misc/int_conv.hpp>              // IWYU pragma: export
#include <base/misc/simple_char_classifications.hpp>
#include <base/preproc/argument_splitter.hpp>  // IWYU pragma: export
#include <base/types/ok_bad.hpp>

#include <type_traits>  // IWYU pragma: export
#include <utility>      // IWYU pragma: export

namespace base::internal {
	template<typename EnumType>
	using StrToEnumType = base::Map<std::string_view, EnumType>;

	template<typename EnumType>
	using EnumToStrType = base::Map<EnumType, std::string_view>;

	/**
	 * Validates that VA_ARGS passed to `MAKE_STRINGIFYABLE_ENUM` are valid, that is
	 * that do not define any non-default values.
	 */
	constexpr OkBad validateStrEnumVaArgs(std::string_view va_args) {
		char previous = ' ';
		for (auto c: va_args) {
			if (c == '-') return BAD;  // '-' is not allowed
			if (c == '=') return BAD;  // '=' default values are not allowed
			if (base::isDigit(c)) {
				if (not base::isAlnum(previous)) {
					// there is a digit that is not a part of identifier:
					return BAD;
				}
			}
			previous = c;
		}
		return OK;
	}
}

/**
 * @brief Creates functions to convert an enum to/from string.
 *
 * @note This macro has to be used in global namespace for technical reasons.
 *
 * @note Writing source docs for these enums is very tricky.
 * See compiler::tsh::Kind or other usages for an example.
 */
#define MAKE_STRINGIFYABLE_ENUM(namespace_name, base_type, name, ...)                            \
	static_assert(std::is_integral_v<base_type>, "base_type must be integral");                  \
	static_assert(not std::is_same_v<base_type, bool>, "base_type must not be bool");            \
	static_assert(                                                                               \
		std::is_same_v<base_type, std::remove_cvref_t<base_type>>,                               \
		"Base type must not be cv-ref qualified"                                                 \
	);                                                                                           \
	static_assert(                                                                               \
		base::internal::validateStrEnumVaArgs(#__VA_ARGS__).isOk(),                              \
		"Default values are not allowed in MAKE_STRINGIFYABLE_ENUM"                              \
	);                                                                                           \
                                                                                                 \
	namespace namespace_name {                                                                   \
		enum class name : base_type { __VA_ARGS__ __VA_OPT__(, ) COUNT };                        \
                                                                                                 \
		namespace name##enum_helper {                                                            \
			constexpr auto ENUM_ELEMENT_COUNT = std::to_underlying(namespace_name::name::COUNT); \
			inline ::base::internal::StrToEnumType<namespace_name::name> strToEnumMaker() {      \
				static const auto string_vector = ::base::vaArgSplit(#__VA_ARGS__);              \
				::base::internal::StrToEnumType<namespace_name::name> out;                       \
				CORE_ASSERT(                                                                     \
					string_vector.size() == base::safeIntConv<usize>(ENUM_ELEMENT_COUNT),        \
					"Enum has different number of values then vaArgSplit provided"               \
				);                                                                               \
				for (base_type i = 0; i < ENUM_ELEMENT_COUNT; i++) {                             \
					out.put(                                                                     \
						std::string_view(string_vector.at(base::safeIntConv<usize>(i))),         \
						static_cast<namespace_name::name>(i)                                     \
					);                                                                           \
				}                                                                                \
				return out;                                                                      \
			}                                                                                    \
			inline ::base::internal::EnumToStrType<namespace_name::name> enumToStrMaker() {      \
				static const auto string_vector = ::base::vaArgSplit(#__VA_ARGS__);              \
				::base::internal::EnumToStrType<namespace_name::name> out;                       \
				CORE_ASSERT(                                                                     \
					string_vector.size() == base::safeIntConv<usize>(ENUM_ELEMENT_COUNT),        \
					"Enum has different number of values then vaArgSplit provided"               \
				);                                                                               \
				for (base_type i = 0; i < ENUM_ELEMENT_COUNT; i++) {                             \
					out.put(                                                                     \
						static_cast<namespace_name::name>(i),                                    \
						std::string_view(string_vector.at(base::safeIntConv<usize>(i)))          \
					);                                                                           \
				}                                                                                \
				return out;                                                                      \
			}                                                                                    \
		}                                                                                        \
	}                                                                                            \
                                                                                                 \
	namespace base {                                                                             \
		template<>                                                                               \
		inline ::namespace_name::name strToEnum<::namespace_name::name>(std::string_view str) {  \
			static ::base::internal::StrToEnumType<namespace_name::name> mapping                 \
				= namespace_name::name##enum_helper::strToEnumMaker();                           \
			CORE_ASSERT(mapping.contains(str), "Enum value not found");                          \
			return mapping[str];                                                                 \
		}                                                                                        \
		template<>                                                                               \
		inline std::string_view enumToStr<::namespace_name::name>(::namespace_name::name v) {    \
			static ::base::internal::EnumToStrType<::namespace_name::name> mapping               \
				= namespace_name::name##enum_helper::enumToStrMaker();                           \
			CORE_ASSERT(mapping.contains(v), "Enum value not found");                            \
			return mapping[v];                                                                   \
		}                                                                                        \
	}

namespace base {
	template<typename EnumType>
	EnumType strToEnum(std::string_view str);

	template<typename EnumType>
	std::string_view enumToStr(EnumType v);
}

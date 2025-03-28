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

#include "maps.hpp"
#include "string_id.hpp"

#include <utility>      // IWYU pragma: export
#include <type_traits>  // IWYU pragma: export

namespace base::detail {
	template<typename EnumType>
	using StrToEnumType = base::Map<base::StrID, EnumType>;

	template<typename EnumType>
	using EnumToStrType = base::Map<EnumType, base::StrID>;
}

/**
 * @brief Creates functions to convert an enum to/from string.
 *
 * @note This macro has to be used in global namespace for technical reasons.
 *
 * @note Writing source docs for these enums is very tricky.
 * See tsh::Kind or other usages for an example.
 */
#define MAKE_STRINGIFYABLE_ENUM(namespace_name, base_type, name, ...)                       \
	static_assert(std::is_integral_v<base_type>, "base_type must be integral");             \
	static_assert(not std::is_same_v<base_type, bool>, "base_type must not be bool");       \
	static_assert(                                                                          \
		std::is_same_v<base_type, std::remove_cvref_t<base_type>>,                          \
		"Base type must not be cv-ref qualified"                                            \
	);                                                                                      \
                                                                                            \
	namespace namespace_name {                                                              \
		enum class name : base_type { __VA_ARGS__ __VA_OPT__(, ) COUNT };                   \
                                                                                            \
		namespace name##enum_helper {                                                       \
			constexpr base_type ENUM_ELEMENT_COUNT                                          \
				= std::to_underlying(namespace_name::name::COUNT);                          \
			inline ::base::detail::StrToEnumType<namespace_name::name> strToEnumMaker() {   \
				auto string_vector = ::base::vaArgSplit(#__VA_ARGS__);                      \
				::base::detail::StrToEnumType<namespace_name::name> out;                    \
				CORE_ASSERT(                                                                \
					string_vector.size() == ENUM_ELEMENT_COUNT,                             \
					"Enum has different number of values then vaArgSplit provided"          \
				);                                                                          \
				for (base_type i = 0; i < ENUM_ELEMENT_COUNT; i++) {                        \
					out.put(                                                                \
						base::StrID(string_vector.at(i).data()),                            \
						static_cast<namespace_name::name>(i)                                \
					);                                                                      \
				}                                                                           \
				return out;                                                                 \
			}                                                                               \
			inline ::base::detail::EnumToStrType<namespace_name::name> enumToStrMaker() {   \
				auto string_vector = ::base::vaArgSplit(#__VA_ARGS__);                      \
				::base::detail::EnumToStrType<namespace_name::name> out;                    \
				CORE_ASSERT(                                                                \
					string_vector.size() == ENUM_ELEMENT_COUNT,                             \
					"Enum has different number of values then vaArgSplit provided"          \
				);                                                                          \
				for (base_type i = 0; i < ENUM_ELEMENT_COUNT; i++) {                        \
					out.put(                                                                \
						static_cast<namespace_name::name>(i),                               \
						::base::StrID(string_vector.at(i).data())                           \
					);                                                                      \
				}                                                                           \
				return out;                                                                 \
			}                                                                               \
		}                                                                                   \
	}                                                                                       \
                                                                                            \
	namespace base {                                                                        \
		template<>                                                                          \
		inline ::namespace_name::name strToEnum<::namespace_name::name>(::base::StrID id) { \
			static ::base::detail::StrToEnumType<namespace_name::name> mapping              \
				= namespace_name::name##enum_helper::strToEnumMaker();                      \
			CORE_ASSERT(mapping.contains(id), "Enum value not found");                  \
			return mapping[id];                                                             \
		}                                                                                   \
		template<>                                                                          \
		inline ::base::StrID enumToStr<::namespace_name::name>(::namespace_name::name v) {  \
			static ::base::detail::EnumToStrType<::namespace_name::name> mapping            \
				= namespace_name::name##enum_helper::enumToStrMaker();                      \
				CORE_ASSERT(mapping.contains(v), "Enum value not found");\
			return mapping[v];                                                              \
		}                                                                                   \
	}

namespace base {
	template<typename EnumType>
	EnumType strToEnum(StrID id);

	template<typename EnumType>
	StrID enumToStr(EnumType v);
}

/**
 * @file stringifyable_enum.hpp
 *
 * @brief This library provides a simple way of creating `enum class` types,
 * that can be automatically converted to `base::StrId` and vice versa.
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
 * Conversion to and from StrId
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

#include "define_helper.hpp"
#include "maps.hpp"
#include "string_id.hpp"

// @TODO: add tests

namespace base {
	namespace detail {
		template<typename EnumType>
		using StrToEnumType = base::Map<base::StrId, EnumType>;

		template<typename EnumType>
		using EnumToStrType = base::Map<EnumType, base::StrId>;

		template<typename EnumType>
		struct TwoMaps {
			EnumToStrType<EnumType> to_str;
			EnumToStrType<EnumType> to_enum;
		};
	}
}

/**
 * @brief Creates functions to convert an enum to/from string.
 *
 * @note This macro has to be used in global namespace for technical reasons.
 */
#define MAKE_STRINGIFYABLE_ENUM(namespace_name, base_type, name, ...)                            \
                                                                                                 \
	namespace namespace_name {                                                                   \
		enum class name : base_type { __VA_ARGS__, COUNT };                                      \
	}                                                                                            \
                                                                                                 \
	namespace name##_enum_helper {                                                               \
		inline ::base::detail::StrToEnumType<namespace_name::name> strToEnumMaker() {            \
			auto string_vector = ::base::vaArgSplit(#__VA_ARGS__);                               \
			::base::detail::StrToEnumType<namespace_name::name> out;                             \
			for (base_type i = 0; i < string_vector.size(); i++) {                               \
				out.put(                                                                         \
					base::StrId(string_vector[i].data()), static_cast<namespace_name::name>(i)   \
				);                                                                               \
			}                                                                                    \
			return out;                                                                          \
		}                                                                                        \
		inline ::base::detail::EnumToStrType<namespace_name::name> enumToStrMaker() {            \
			auto string_vector = ::base::vaArgSplit(#__VA_ARGS__);                               \
			::base::detail::EnumToStrType<namespace_name::name> out;                             \
			for (base_type i = 0; i < string_vector.size(); i++) {                               \
				out.put(                                                                         \
					static_cast<namespace_name::name>(i), ::base::StrId(string_vector[i].data()) \
				);                                                                               \
			}                                                                                    \
			return out;                                                                          \
		}                                                                                        \
	}                                                                                            \
                                                                                                 \
	namespace base {                                                                             \
		template<>                                                                               \
		inline ::namespace_name::name strToEnum<::namespace_name::name>(::base::StrId id) {      \
			static ::base::detail::StrToEnumType<namespace_name::name> mapping                   \
				= name##_enum_helper::strToEnumMaker();                                          \
			return mapping[id];                                                                  \
		}                                                                                        \
		template<>                                                                               \
		inline ::base::StrId enumToStr<::namespace_name::name>(::namespace_name::name v) {       \
			static ::base::detail::EnumToStrType<::namespace_name::name> mapping                 \
				= name##_enum_helper::enumToStrMaker();                                          \
			return mapping[v];                                                                   \
		}                                                                                        \
	}

namespace base {
	template<typename EnumType>
	EnumType strToEnum(StrId id);

	template<typename EnumType>
	StrId enumToStr(EnumType v);
}

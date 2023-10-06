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
 * @brief This macro has to be used in global namespace for technical reasons
 */
#define MAKE_STRINGFYABLE_ENUM(namespace_name, base_type, name, ...)                             \
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

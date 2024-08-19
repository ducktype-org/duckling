#pragma once

namespace json {
	// Here it says template<class> (without a T), so that IsEmptySerialization is a template,
	// and we can provide a specialization for a type.
	template<class>
	struct IsEmptySerialization {
		static constexpr bool value = false;
	};
}

template<>
struct json::IsEmptySerialization<void> {
	static constexpr bool value = false;
};

#define JSON_REGISTER_EMPTY_STRUCT_WITH_NAME(T, NAME)                                 \
	namespace json {                                                                  \
		template<>                                                                    \
		struct IsEmptySerialization<T> {                                              \
			static constexpr bool value = true;                                       \
		};                                                                            \
	}                                                                                 \
	JSON_REGISTER_TYPE_WITH_NAME(T, NAME);                                            \
	template<>                                                                        \
	struct nlohmann::adl_serializer<T> {                                              \
		static void to_json(json& j, const T&) {                                      \
			j["type"] = std::string(TypeParseTraits<T>::name.data());                 \
		}                                                                             \
		static void from_json(const json&, T&) {                                      \
			RIFT_PANIC("Parsing data from JSON into " #T " is not supported (yet)."); \
		}                                                                             \
	};

#define JSON_REGISTER_EMPTY_STRUCT(T) JSON_REGISTER_EMPTY_STRUCT_WITH_NAME(T, #T)

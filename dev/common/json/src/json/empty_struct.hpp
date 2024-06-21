#pragma once

namespace nlohmann {
	template<class>
	struct IsEmptySerialization {
		static constexpr bool value = false;
	};
}

#define NLOHMANN_EMPTY_STRUCT(T)                  \
	template<>                                    \
	struct nlohmann::adl_serializer<T> {          \
		static void to_json(json&, const T&) {}   \
		static void from_json(const json&, T&) {} \
	};                                            \
	namespace nlohmann {                          \
		template<>                                \
		struct IsEmptySerialization<T> {          \
			static constexpr bool value = true;   \
		};                                        \
	}

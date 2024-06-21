#pragma once

namespace nlohmann {
	template<class>
	struct IsEmptySerialization {
		static constexpr bool value = false;
	};
}

#define NLOHMANN_EMPTY_STRUCT(T)                \
	namespace nlohmann {                        \
		template<>                              \
		struct IsEmptySerialization<T> {        \
			static constexpr bool value = true; \
		};                                      \
	}

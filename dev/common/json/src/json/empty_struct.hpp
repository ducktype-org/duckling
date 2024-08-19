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

#define NLOHMANN_EMPTY_STRUCT(T)                \
	namespace json {                            \
		template<>                              \
		struct IsEmptySerialization<T> {        \
			static constexpr bool value = true; \
		};                                      \
	}

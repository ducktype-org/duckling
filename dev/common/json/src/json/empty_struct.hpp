#pragma once

#include "helper.hpp"
#include <json_struct/json_struct.h>

namespace JS {
	template<class T>
	struct IsEmptySerialization {
		static constexpr const bool value = false;
	};
}

namespace JS {
	template<>
	struct IsEmptySerialization<void> {
		static constexpr const bool value = false;
	};
}

#define JS_EMPTY(T)                                                                          \
	namespace JS {                                                                           \
		template<>                                                                           \
		class TypeHandler<T> {                                                               \
		public:                                                                              \
			static inline Error                                                              \
				to([[maybe_unused]] T &to, [[maybe_unused]] ParseContext &context) {         \
				return Error::NoError;                                                       \
			}                                                                                \
                                                                                             \
			static void                                                                      \
				from([[maybe_unused]] const T &from, Token &token, Serializer &serializer) { \
				impl::emptyObject(token, serializer);                                        \
			}                                                                                \
		};                                                                                   \
		template<>                                                                           \
		struct IsEmptySerialization<T> {                                                     \
			static constexpr const bool value = true;                                        \
		};                                                                                   \
	}

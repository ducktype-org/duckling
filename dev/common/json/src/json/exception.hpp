#pragma once

#include <exception>
#include <json_struct/json_struct.h>

namespace JS {
	template<>
	class TypeHandler<std::exception> {
	public:
		static inline Error to([[maybe_unused]] std::exception& to,
		                       [[maybe_unused]] ParseContext&   context) {
			// @TODO?
			return Error::NoError;
		}

		static void from(const std::exception& from, Token& token, Serializer& serializer) {
			static const char exceptionName[] = "error";

			impl::beginObject(token, serializer);

			token.name      = DataRef(exceptionName);
			token.name_type = Type::String;
			TypeHandler<std::string>::from(from.what(), token, serializer);

			impl::endObject(token, serializer);
		}
	};
}

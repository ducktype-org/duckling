#include "helper.hpp"

#include <array>

namespace JS::impl {
	void beginObject(Token& token, Serializer& serializer) {
		static const std::array<char, 1> objectStart{ '{' };
		token.value_type = Type::ObjectStart;
		token.value      = DataRef(objectStart.begin());
		serializer.write(token);
	}

	void endObject(Token& token, Serializer& serializer) {
		static const std::array<char, 1> objectEnd{ '}' };
		token.name.size  = 0;
		token.name.data  = "";
		token.name_type  = Type::String;
		token.value_type = Type::ObjectEnd;
		token.value      = DataRef(objectEnd.begin());
		serializer.write(token);
	}

	void emptyObject(Token& token, Serializer& serializer) {
		beginObject(token, serializer);
		endObject(token, serializer);
	}
}

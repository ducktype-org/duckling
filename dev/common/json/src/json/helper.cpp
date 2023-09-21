#include "helper.hpp"

namespace JS::impl {
	void beginObject(Token& token, Serializer& serializer) {
		static const char objectStart[] = "{";
		token.value_type = Type::ObjectStart;
		token.value = DataRef(objectStart);
		serializer.write(token);
	}
	void endObject(Token& token, Serializer& serializer) {
		static const char objectEnd[] = "}";
		token.name.size = 0;
		token.name.data = "";
		token.name_type = Type::String;
		token.value_type = Type::ObjectEnd;
		token.value = DataRef(objectEnd);
		serializer.write(token);
	}
	void emptyObject(Token& token, Serializer& serializer) {
		beginObject(token, serializer);
		endObject(token, serializer);
	}
}

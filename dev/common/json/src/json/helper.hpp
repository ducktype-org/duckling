#pragma once

#include <json_struct/json_struct.h>

namespace JS::impl {
	void beginObject(Token& token, Serializer& serializer);
	void endObject(Token& token, Serializer& serializer);
	void emptyObject(Token& token, Serializer& serializer);
}  // namespace JS::impl

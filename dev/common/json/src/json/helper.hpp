#pragma once

#pragma GCC diagnostic push
#if defined(__GNUC__) && (!defined(__clang__))
	#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif
#include <json_struct/json_struct.h>
#pragma GCC diagnostic pop

namespace JS::impl {
	void beginObject(Token& token, Serializer& serializer);
	void endObject(Token& token, Serializer& serializer);
	void emptyObject(Token& token, Serializer& serializer);
}

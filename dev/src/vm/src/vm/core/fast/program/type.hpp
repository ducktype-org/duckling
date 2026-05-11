#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

namespace vm::fast {
	STRONG_TYPEDEF_ID_DIRECT_CREATION(TypeID);

	struct Type {
		base::StrID name;
		Bytes       size;
	};
}

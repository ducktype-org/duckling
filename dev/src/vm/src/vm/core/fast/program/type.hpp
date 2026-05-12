#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

namespace vm::fast {
	STRONG_TYPEDEF_ID_DIRECT_CREATION(TypeID);

	struct VariantData {
		// For variant types, the size of the type tag in bytes.
		Bytes               type_tag_size;
		std::vector<TypeID> variant_alternatives;
	};

	struct Type {
		base::StrID name;
		Bytes       size;
		union {
			VariantData variant_data;
		};
	};
}

#pragma once

#include <base/types/bits_and_bytes.hpp>

#include <string_id/string_id.hpp>

namespace vm::fast {
	struct VariantData {
		// For variant types, the size of the type tag in bytes.
		Bytes type_tag_size;
	};

	struct Type {
		base::StrID name;
		Bytes       size;

		union {
			VariantData variant_data;
		};
	};
}

#pragma once

#include <base/collections/optional.hpp>
#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include <functional>
#include <string>

namespace dia_int {
	struct CodeLocation {
		std::string         file;
		u64                 line;
		u64                 column;
		base::Optional<u64> end_line;
		base::Optional<u64>
			end_column;  // Optional hash of the PST node corresponding to this code location.
	};

	struct HashCodeLocation {
		base::Bit256                 begin_node;
		base::Optional<base::Bit256> end_node;
	};

	using UpdatePositionFunc = std::function<CodeLocation(const HashCodeLocation&)>;
}

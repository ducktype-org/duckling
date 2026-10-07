// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/optional.hpp>
#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>

#include <functional>
#include <string>

namespace dia {
	struct CodeLocation final {
		std::string         file{};
		u64                 line{};
		u64                 column{};
		base::Optional<u64> end_line;
		base::Optional<u64>
			end_column;  // Optional hash of the PST node corresponding to this code location.
	};

	struct HashCodeLocation final {
		base::Bit256                 begin_node;
		base::Optional<base::Bit256> end_node;
	};

	using UpdatePositionFunc = std::function<CodeLocation(const HashCodeLocation&)>;
}

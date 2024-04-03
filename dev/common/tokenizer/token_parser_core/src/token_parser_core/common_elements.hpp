#pragma once

#include <base/string_id.hpp>
#include <base/optional.hpp>

namespace tpc {
	/**
	 * @brief Struct for storing identifiers
	 */
	struct Identifier {
		base::StrId value;

		operator base::StrId() { return value; }
	};

	/**
	 * @brief Struct for storing optional identifiers
	 */
	struct OptionalIdentifier {
		base::Optional<base::StrId> value;
	};
}

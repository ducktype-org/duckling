#pragma once

#include <base/string_id.hpp>
#include <base/optional.hpp>
#include <diagnostic/source_position.hpp>

namespace tpc {
	/**
	 * @brief Struct for storing identifiers
	 */
	struct Identifier {
		base::StrId         value;
		dia::SourcePosition position = dia::SourcePosition::getBadPosition();

		operator base::StrId() { return value; }
	};

	/**
	 * @brief Struct for storing optional identifiers
	 */
	struct OptionalIdentifier {
		base::Optional<base::StrId> value;
		dia::SourcePosition         position = dia::SourcePosition::getBadPosition();
	};
}

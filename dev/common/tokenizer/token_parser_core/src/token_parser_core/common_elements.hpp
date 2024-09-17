#pragma once

#include <base/string_id.hpp>
#include <base/optional.hpp>
#include <diagnostic/source_position.hpp>

namespace tpc {
	/**
	 * @brief Struct for storing identifiers
	 */
	struct Identifier {
		base::StrID value;

		// @TODO: this should be changed do be properly set during parsing:
		dia::SourcePosition position = dia::SourcePosition::fakePosition();

		operator base::StrID() { return value; }
	};

	/**
	 * @brief Struct for storing optional identifiers
	 */
	struct OptionalIdentifier {
		base::Optional<base::StrID> value;

		// @TODO: this should be changed do be properly set during parsing:
		dia::SourcePosition position = dia::SourcePosition::fakePosition();
	};
}

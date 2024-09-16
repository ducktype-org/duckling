#pragma once

#include <base/string_id.hpp>
#include <base/optional.hpp>
#include <diagnostic/source_position.hpp>

namespace tpc {
	/**
	 * @brief Struct for storing identifiers
	 */
	struct Identifier final {
		base::StrId value;

		// @TODO: this should be changed do be properly set during parsing:
		dia::SourcePosition position = dia::SourcePosition::fakePosition();

		operator base::StrId() { return value; }
	};

	/**
	 * @brief Struct for storing optional identifiers
	 */
	struct OptionalIdentifier final {
		base::Optional<base::StrId> value;

		// @TODO: this should be changed do be properly set during parsing:
		dia::SourcePosition position = dia::SourcePosition::fakePosition();
	};
}

#pragma once

#include <base/optional.hpp>
#include <base/string_id.hpp>

#include <diagnostic/source_position.hpp>
#include <lexer/token.hpp>

namespace tpc {
	/**
	 * @brief Struct for storing identifiers
	 */
	struct Identifier final {
		base::StrID value;

		// @TODO: this should be changed do be properly set during parsing:
		dia::SourcePosition position = dia::SourcePosition::fakePosition();

		operator base::StrID() { return value; }
	};

	/**
	 * @brief Struct for storing optional identifiers
	 */
	struct OptionalIdentifier final {
		base::Optional<base::StrID> value;

		// @TODO: this should be changed do be properly set during parsing:
		dia::SourcePosition position = dia::SourcePosition::fakePosition();
	};
}

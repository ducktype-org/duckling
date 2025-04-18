#pragma once

#include <diagnostic/source_position.hpp>
#include <lexer/token.hpp>

#include <base/optional.hpp>
#include <base/string_id.hpp>

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

	/**
	 * @brief Struct for storing string value
	 */
	struct StringValue final {
		base::StrID value;

		StringValue(): value(base::StrID("")) {}

		StringValue(const base::StrID id): value(id) {}

		StringValue(const std::string& str): value(base::StrID(str.c_str())) {}

		StringValue(const StringValue&) = default;

		operator base::StrID() { return value; }

		/**
		 * @brief Returns the actual unescaped contents
		 */
		[[nodiscard]]
		std::string str() const {
			std::stringstream ss;
			ss << value.str();
			return ss.str();
		}

		/**
		 * @note This should probably do something more in the future
		 */
		inline bool operator==(StringValue& other) { return str() == other.str(); }
	};
}

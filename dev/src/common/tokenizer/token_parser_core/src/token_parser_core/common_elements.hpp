#pragma once

#include <base/misc/optional.hpp>
#include <base/str/string_id.hpp>

#include <diagnostic/source_position.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <lexer/token.hpp>

namespace tpc {
	/**
	 * @brief Struct for storing identifiers
	 */
	struct Identifier final {
		base::StrID value;

		// @TODO: this should be changed do be properly set during parsing:
		dia::SourcePosition position = dia::SourcePosition::fakePosition();

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const Identifier& t
		) noexcept {
			addToHash(h, t.value.strView());
		}

		operator base::StrID() { return value; }
	};

	/**
	 * @brief Struct for storing optional identifiers
	 */
	struct OptionalIdentifier final {
		base::Optional<base::StrID> value;

		// @TODO: this should be changed do be properly set during parsing:
		dia::SourcePosition position = dia::SourcePosition::fakePosition();

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const OptionalIdentifier& t
		) noexcept {
			addToHash(h, t.value.has_value());
			if (t.value) addToHash(h, t.value->strView());
		}
	};

	/**
	 * @brief Struct for storing string value
	 *
	 * @note This should work for now with basic characters and c++ escape sequences.
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
		 * @note This should probably do something more in the future.
		 */
		bool operator==(StringValue& other) { return str() == other.str(); }

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const StringValue& t
		) noexcept {
			addToHash(h, t.value.str());
		}
	};

	/**
	 * @brief Struct for storing string value
	 *
	 * @note This should work for now with basic characters and c++ escape sequences
	 */
	struct CharValue final {
		base::StrID value;

		CharValue(): value(base::StrID("!")) {}

		CharValue(const base::StrID id): value(id) {}

		CharValue(const std::string& str): value(base::StrID(str.c_str())) {}

		CharValue(const CharValue&) = default;

		operator base::StrID() { return value; }

		/**
		 * @brief Returns the actual unescaped contents
		 */
		[[nodiscard]]
		char charValue() const {
			std::stringstream ss;
			ss << value.str();
			return ss.str()[0];
		}

		/**
		 * @note This should probably do something more in the future
		 */
		bool operator==(CharValue& other) { return charValue() == other.charValue(); }

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const CharValue& t
		) noexcept {
			addToHash(h, t.value.strView());
		}
	};
}

// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/optional.hpp>

#include <diagnostic/source_position.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <lexer/token.hpp>
#include <string_id/string_id.hpp>

#include <sstream>

namespace tpc {
	/**
	 * @brief Struct for storing identifiers
	 */
	struct Identifier final {
		base::StrID value;

		dia::SourcePosition position = dia::SourcePosition::fakePosition();

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const Identifier& t
		) noexcept {
			addToHash(h, t.value.strView());
		}

		operator base::StrID() { return value; }
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

		StringValue(const StringValue&)            = default;
		StringValue& operator=(const StringValue&) = default;

		operator base::StrID() { return value; }

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

	/**
	 * @brief Struct for storing a numeric literal with an optional type specifier.
	 */
	struct NumericValue final {
		base::StrID                 value;
		base::Optional<base::StrID> type_specifier{};

		NumericValue(): value(base::StrID("")) {}

		NumericValue(const base::StrID id): value(id) {}

		NumericValue(base::StrID id, base::Optional<base::StrID> type_specifier):
			  value(id),
			  type_specifier(type_specifier) {}

		NumericValue(const NumericValue&) = default;

		[[nodiscard]]
		std::string str() const {
			std::stringstream ss;
			ss << value.str();
			if (type_specifier.has_value()) ss << type_specifier->str();
			return ss.str();
		}

		/**
		 * @note This should probably do something more in the future.
		 */
		bool operator==(NumericValue& other) { return str() == other.str(); }

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const NumericValue& t
		) noexcept {
			addToHash(h, t.value);
			if (t.type_specifier.has_value()) addToHash(h, t.type_specifier.value());
		}
	};
}

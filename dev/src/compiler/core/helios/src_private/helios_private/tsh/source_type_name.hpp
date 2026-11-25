// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <query_framework/context/context_fd.hpp>

#include <string>

namespace compiler::tsh {
	/**
	 * @brief Binding strength of a rendered type name, ordered from the loosest to the tightest.
	 */
	enum class TypeNamePrecedence {
		Function,
		Variant,
		Prefix,
		Postfix,
		Atomic,
	};

	/**
	 * @brief A rendered source type name together with its precedence.
	 */
	struct SourceTypeName final {
		std::string        text;
		TypeNamePrecedence precedence;

		/**
		 * @brief Returns the text, parenthesized when it binds looser than @p parent_precedence.
		 */
		[[nodiscard]] std::string nestedIn(const TypeNamePrecedence parent_precedence) const {
			return precedence < parent_precedence ? "(" + text + ")" : text;
		}
	};

	/**
	 * @brief Renders the source name of @p type, as documented by AbstractType::sourceTypeName.
	 */
	SourceTypeName renderAbstractTypeName(query::Context& ctx, AbstractType type);

	/**
	 * @brief Renders the source name of @p type, as documented by SymbolType::sourceTypeName.
	 */
	SourceTypeName renderSymbolTypeName(query::Context& ctx, SymbolType<> type);
}

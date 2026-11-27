// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../../cloning/cloning_automatic.hpp"  // IWYU pragma: export
#include "../../state/lang_parser_state.hpp"
#include "../../visitors/pst_expr_visitor.hpp"  // IWYU pragma: export
#include "../../visitors/pst_visitor.hpp"       // IWYU pragma: export
#include "../common/parser_common_errors.hpp"   // IWYU pragma: export
#include "../includes/basic.hpp"                // IWYU pragma: export

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/message.hpp>
#include <lang_definitions/key_spec_op.hpp>
#include <lang_definitions/operator_precedence.hpp>
#include <lexer/token.hpp>
#include <token_parser_core/automatic.hpp>
#include <unicode_classification/classifications.hpp>

#include <functional>  // IWYU pragma: export

namespace pst {
	using lang_def::Keyword;
	using lang_def::NamedOperator;
	using lang_def::Special;
	using lexer::Operator;

	using lexer::Token;

	/**
	 * @note This error that should generally not happen outside of our errors.
	 */
	template<typename Type>
	class BadStatementChoice final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_statement_choice" };
		}

	public:
		BadStatementChoice(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("type_name", std::string(base::typeName<Type>()));
		}
	};

	template<typename Type>
	bool assertStmtChoice(LangParserState& state, bool good) {
		if (!good)
			state.logInt(makeBox<BadStatementChoice<Type>>(state.ctokens().peek().getPosition()));
		return good;
	}

	/**
	 * @brief Checks if the token @p at tokens ahead names a bracket operator, like the `()` of
	 * `fun ()(x: i64)` or the `[]` of `fun [](i: i64)`: a round or square bracket group followed by
	 * the parameter list.
	 */
	inline bool isBracketOperatorName(LangParserState& state, i64 at) {
		return (state[at].isBracketGroup(Token::Round) || state[at].isBracketGroup(Token::Square))
		    && state[at + 1].isBracketGroup(Token::Round);
	}

	/**
	 * @brief Consumes a contextual operator fixity specifier when it precedes an operator name.
	 *
	 * `prefix` and `suffix` remain regular identifiers everywhere else, including as function
	 * names. The bracket operators `()` and `[]` have no fixity, so a specifier before them is
	 * reported and skipped.
	 */
	inline OperatorFixity parseOperatorFixity(LangParserState& state) {
		const bool bracket_operator = isBracketOperatorName(state, 1);
		if (!state[0].isIdentifier() || !(state[1].isOperatorSymbol() || bracket_operator))
			return OperatorFixity::None;

		const auto fixity_str = state[0].getStrValue();
		if (fixity_str != "prefix" && fixity_str != "suffix") return OperatorFixity::None;

		if (bracket_operator) {
			state.logInt(makeBox<error::BracketOperatorFixityError>(state.getPosition()));
			state.tokens().next();
			return OperatorFixity::None;
		}

		state.tokens().next();
		return fixity_str == "prefix" ? OperatorFixity::Prefix : OperatorFixity::Suffix;
	}
}

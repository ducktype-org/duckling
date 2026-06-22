#pragma once

#include "../../cloning_automatic.hpp"  // IWYU pragma: export
#include "../../lang_parser_state.hpp"
#include "../../pst_expr_visitor.hpp"   // IWYU pragma: export
#include "../../pst_visitor.hpp"        // IWYU pragma: export
#include "../includes/basic.hpp"        // IWYU pragma: export
#include "../parser_common_errors.hpp"  // IWYU pragma: export

#include <diagnostic_interactive/message.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

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
	class BadStatementChoice final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_statement_choice" };
		}

	public:
		BadStatementChoice(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia_int::TextArgument>("type_name", std::string(base::typeName<Type>()));
		}
	};

	template<typename Type>
	bool assertStmtChoice(LangParserState& state, bool good) {
		if (!good)
			state.logInt(makeBox<BadStatementChoice<Type>>(state.ctokens().peek().getPosition()));
		return good;
	}
}

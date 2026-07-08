#pragma once

#include "../../hierarchy/not_statements/wrapper_elements/identifier_wrapper.hpp"
#include "preamble.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst {

	/**
	 * @brief Error for operators that cannot be used as function names.
	 */
	class ReservedOperatorFunNameError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "reserved_operator_fun_name" };
		}

	public:
		ReservedOperatorFunNameError(dia::SourcePosition pos, std::string op):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia_int::TextArgument>("operator", std::move(op));
		}
	};

	/**
	 * @brief Error for custom assignment operators, which are not supported yet.
	 */
	class AssignmentOperatorFunNameError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "assignment_operator_fun_name" };
		}

	public:
		AssignmentOperatorFunNameError(dia::SourcePosition pos, std::string op):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia_int::TextArgument>("operator", std::move(op));
		}
	};

	/**
	 * @brief Parses a function name: an identifier or a non-reserved operator symbol.
	 */
	inline MBox<IdentifierWrapper> parseFunctionName(LangParserState& state) {
		if (!state[0].isOperatorSymbol()) return IdentifierWrapper::parse(state);

		auto op   = state[0].asBinaryOperator().value();
		auto name = state[0].getValue();

		if (!op.isNotReserved()) {
			// Plain `=` has no base operator to suggest, so it counts as reserved.
			if (op.isAssignment() && op.asNamed() != lang_def::NamedOperator::Assign)
				state.logInt(makeBox<AssignmentOperatorFunNameError>(state.getPosition(), op.str()));
			else
				state.logInt(makeBox<ReservedOperatorFunNameError>(state.getPosition(), op.str()));
			name = base::StrID("bad identifier");
		}

		Box<IdentifierWrapper> out = makeBox<IdentifierWrapper>(state, name);

		PARSE().eatOne();

		PST_RETURN out;
	}

}

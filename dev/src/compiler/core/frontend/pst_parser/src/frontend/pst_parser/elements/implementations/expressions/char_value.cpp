#include "../../hierarchy/expressions/char_value.hpp"

#include "preamble.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst::expr {
	/**
	 * @brief For now this is a safety error (meaning it should never happen), unless there will be
	 * some situation where only a string value will be accepted in an expression.
	 */
	class BadCharValueError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_char_value_error" };
		}

	public:
		BadCharValueError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MoreThanCharValueError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "more_than_char_value_error" };
		}

	public:
		MoreThanCharValueError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	MBox<ExprElement> ExprCharValue::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].isChar()) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadCharValueError>(pos));
			fastForward(state, length);
			return nullptr;
		}

		auto out = makeBox<ExprCharValue>(pos, state[0].getValue());
		state.parse(out).eatOne();

		if (length > 1) {
			state.logInt(makeBox<MoreThanCharValueError>(pos));
			fastForward(state, length);
		}

		return out;
	}

	void ExprCharValue::dprint(std::ostream& out) const {
		out << "{";

		out << R"("char": ")" << string.value.str() << "\"";

		out << "}";
	}

	LangElement::HashAlg& ExprCharValue::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, string);
		return partial_hash;
	}

	void ExprCharValue::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitExprCharValue(*this);
	}
}

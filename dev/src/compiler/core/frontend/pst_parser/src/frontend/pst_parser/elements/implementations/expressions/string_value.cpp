#include "../../hierarchy/expressions/string_value.hpp"

#include "preamble.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst::expr {
	/**
	 * @brief For now this is a safety error (meaning it should never happen), unless there will be
	 * some situation where only a string value will be accepted in an expression.
	 */
	class BadStrValueError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_str_value_error" };
		}

	public:
		BadStrValueError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MoreThanStrValueError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "more_than_str_value_error" };
		}

	public:
		MoreThanStrValueError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	MBox<ExprElement> ExprStrValue::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].isString()) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadStrValueError>(pos));
			fastForward(state, length);
			return nullptr;
		}

		auto out = makeBox<ExprStrValue>(pos, state[0].getValue());
		state.parse(out).eatOne();

		if (length > 1) {
			state.logInt(makeBox<MoreThanStrValueError>(pos));
			fastForward(state, length);
		}

		return out;
	}

	void ExprStrValue::dprint(std::ostream& out) const {
		out << "{";

		out << R"("string": ")" << string.value.str() << "\"";

		out << "}";
	}

	LangElement::HashAlg& ExprStrValue::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, string);
		return partial_hash;
	}

	void ExprStrValue::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitExprStrValue(*this);
	}
}

#include "../../hierarchy/expressions/char_value.hpp"

#include "preamble.hpp"

namespace pst::expr {
	/**
	 * @brief For now this is a safety error (meaning it should never happen), unless there will be
	 * some situation where only a string value will be accepted in an expression.
	 */
	class BadCharValueError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected a Char value";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadCharValueError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class MoreThanCharValueError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected just a single Char value";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		MoreThanCharValueError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> ExprCharValue::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].isChar()) {
			// This should (probably) never happen with how it's called by the parser
			state.log(makeBox<BadCharValueError>(pos));
			fastForward(state, length);
			return nullptr;
		}

		auto out = makeBox<ExprCharValue>(pos, state[0].getValue());
		state.parse(out).eatOne();

		if (length > 1) {
			state.log(makeBox<MoreThanCharValueError>(pos));
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

#include "preamble.hpp"

namespace pst::expr {
	/**
	 * @brief For now this is a safety error unless there will be some situation where only a string
	 * value will be accepted.
	 */
	class BadStrValueError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected a string value";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadStrValueError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class MoreThanStrValueError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected just a single string value";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		MoreThanStrValueError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> ExprStrValue::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].isString()) {
			// This should (probably) never happen with how it's called by the parser
			state.log(makeBox<BadStrValueError>(pos));
			fastForward(state, length);
			return nullptr;
		}

		auto out = makeBox<ExprStrValue>(pos, state[0].getValue());
		state.parse(out).eatOne();

		if (length > 1) {
			state.log(makeBox<MoreThanStrValueError>(pos));
			fastForward(state, length);
		}

		return out;
	}

	void ExprStrValue::dprint(std::ostream& out) const {
		out << "{";

		out << R"("string": ")" << string.value.str() << "\"";

		out << "}";
	}

	void ExprStrValue::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitExprStrValue(*this);
	}
}

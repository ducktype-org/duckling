#include "preamble.hpp"

namespace pst::expr {
	class BadValueError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected a value";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadValueError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class MoreThanValueError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected just a single value";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		MoreThanValueError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> ExprValue::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].is(lexer::Token::Type::NumLiteral)) {
			// This should never happen
			state.log(base::make_unique<BadValueError>(pos));
			fastForward(state, length);
			return nullptr;
		}

		auto out = makeBox<ExprValue>(pos, state[0].getValue());
		state.parse(out).eatOne();

		if (length > 1) {
			state.log(base::make_unique<MoreThanValueError>(pos));
			fastForward(state, length);
		}

		return out;
	}

	void ExprValue::dprint(std::ostream& out) const {
		out << "{";

		out << R"("number": ")" << number.str() << "\"";

		out << "}";
	}

	void ExprValue::acceptVisitor(PstExprVisitor& visitor) const { visitor.visitExprValue(*this); }
}

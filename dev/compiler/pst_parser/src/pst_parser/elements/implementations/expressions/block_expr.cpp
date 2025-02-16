#include "preamble.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst::expr {
	class BadBlockError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected single block expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadBlockError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> BlockExpr::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1 && state[0].isBracketGroup(lexer::Token::Curly))) {
			// This should (probably) never happen with how it's called by the parser
			state.log(makeBox<BadBlockError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<BlockExpr>(state.getPosition());

		state.parse(out).one(&out->block);

		return out;
	}

	void BlockExpr::dprint(std::ostream& out) const {
		out << "{";

		out << R"("block": )";
		nullAwareDprint(block, out);

		out << "}";
	}

	void BlockExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitBlockExpr(*this);
	}
}

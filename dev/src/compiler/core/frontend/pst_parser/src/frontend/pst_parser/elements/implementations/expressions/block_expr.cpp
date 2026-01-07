#include "../../hierarchy/expressions/block_expr.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst::expr {
	class BadBlockError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_block_error" };
		}

	public:
		BadBlockError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	MBox<ExprElement> BlockExpr::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1 && state[0].isBracketGroup(lexer::Token::Curly))) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadBlockError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<BlockExpr>(state.getPosition());

		state.parse(out).withDef(&out->block, CodeBlock::CodeBlockType::Ordered);

		return out;
	}

	void BlockExpr::dprint(std::ostream& out) const {
		out << "{";

		out << R"("block": )";
		nullAwareDprint(block, out);

		out << "}";
	}

	LangElement::HashAlg& BlockExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void BlockExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitBlockExpr(*this);
	}
}

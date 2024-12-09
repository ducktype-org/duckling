#include "preamble.hpp"

namespace pst {
	MBox<CodeBlockOrStmt> CodeBlockOrStmt::parse(LangParserState& state) {
		auto out = makeBox<CodeBlockOrStmt>(state.getPosition());
		if (state[0].isBracketGroup(Token::BracketType::Curly)) {
			MBox<CodeBlock> block;
			state.parse(out).one(&block);
			if (!block) return nullptr;
			out->content = std::move(block);
		} else {
			MBox<Stmt> stmt;
			state.parse(out).one(&stmt);
			if (!stmt) return nullptr;
			out->content = std::move(stmt);
		}

		return out;
	}

	void CodeBlockOrStmt::dprint(std::ostream& out) const {
		auto printThrough = [&](const auto& el) { return tpc::nullAwareDprint(el, out); };

		std::visit(printThrough, content);
	}

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::begin() const {
		variant_match(content) {
			variant_case(MBox<Stmt>, stmt) { return const_iterator(&stmt); }
			variant_case(MBox<CodeBlock>, code_block) { return code_block->begin(); }
		}
		CORE_UNREACHABLE();
	}

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::end() const {
		variant_match(content) {
			variant_case(MBox<Stmt>, stmt) { return const_iterator(&stmt) + 1; }
			variant_case(MBox<CodeBlock>, code_block) { return code_block->end(); }
		}
		CORE_UNREACHABLE();
	}
}

#include "preamble.hpp"

namespace pst {
	MBox<CodeBlockOrStmt> CodeBlockOrStmt::parse(LangParserState& state) {
		auto out = makeBox<CodeBlockOrStmt>(state.getPosition());
		if (state[0].isBracketGroup(Token::BracketType::Curly)) {
			AccessInternal<CodeBlock> block;
			state.parse(out).one(&block);
			if (!block.internal()) return nullptr;
			out->content = std::move(block);
		} else {
			AccessInternal<Stmt> stmt;
			state.parse(out).one(&stmt);
			if (!stmt.internal()) return nullptr;
			out->content = std::move(stmt);
		}

		return out;
	}

	void CodeBlockOrStmt::dprint(std::ostream& out) const {
		auto printThrough = [&](const auto& el) { return nullAwareDprint(el, out); };

		std::visit(printThrough, content);
	}

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::begin() const {
		variant_match(content) {
			variant_case(AccessInternal<Stmt>, stmt) { return const_iterator(&stmt); }
			variant_case(AccessInternal<CodeBlock>, code_block) {
				return code_block.internal()->begin();
			}
		}
		CORE_UNREACHABLE();
	}

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::end() const {
		variant_match(content) {
			variant_case(AccessInternal<Stmt>, stmt) { return const_iterator(&stmt) + 1; }
			variant_case(AccessInternal<CodeBlock>, code_block) {
				return code_block.internal()->end();
			}
		}
		CORE_UNREACHABLE();
	}
}

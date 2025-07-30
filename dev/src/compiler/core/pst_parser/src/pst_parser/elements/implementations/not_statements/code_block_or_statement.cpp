#include "../../hierarchy/not_statements/code_block_or_statement.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<CodeBlockOrStmt> CodeBlockOrStmt::parse(LangParserState& state, CodeBlock::CodeBlockType code_block_order_type) {
		auto out = makeBox<CodeBlockOrStmt>(state.getPosition());
		if (state[0].isBracketGroup(Token::BracketType::Curly)) {
			MBox<CodeBlock> block;
			state.parse(out).with(&block, CodeBlock::parse, fwdVal(code_block_order_type));
			if (!block) return nullptr;
			state.parse(out).assign(&out->code_block, std::move(block));
		} else {
			MBox<Stmt> stmt;
			state.parse(out).one(&stmt);
			if (!stmt) return nullptr;
			state.parse(out).assign(&out->stmt, std::move(stmt));
		}

		return out;
	}

	void CodeBlockOrStmt::dprint(std::ostream& out) const {
		if (code_block) {
			nullAwareDprint(code_block.value(), out);
		} else {
			nullAwareDprint(stmt.value(), out);
		}
	}

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::begin() const {
		if (code_block) {
			return code_block.value().internal()->begin();
		} else {
			return const_iterator(&stmt.value());
		}
		// variant_match(content) {
			// variant_case(AccessInternal<Stmt>, stmt) { return const_iterator(&stmt); }
			// variant_case(AccessInternal<CodeBlock>, code_block) {
				// return code_block.internal()->begin();
			// }
		// }
		// CORE_UNREACHABLE();
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

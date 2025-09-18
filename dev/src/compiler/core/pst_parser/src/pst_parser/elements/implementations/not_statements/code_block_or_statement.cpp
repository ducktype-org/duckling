#include "../../hierarchy/not_statements/code_block_or_statement.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<CodeBlockOrStmt> CodeBlockOrStmt::parse(
		LangParserState& state, CodeBlock::CodeBlockType code_block_order_type
	) {
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
		if (code_block)
			nullAwareDprint(code_block.value(), out);
		else
			nullAwareDprint(stmt.value(), out);
	}

	LangElement::HashAlg& CodeBlockOrStmt::calcStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, code_block.has_value());
		addToHash(partial_hash, stmt.has_value());
		return partial_hash;
	}

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::begin() const {
		if (code_block)
			return code_block.value().internal()->begin();
		else if (stmt)
			return stmt->give();
		CORE_UNREACHABLE();
	}

	CodeBlockOrStmt::const_iterator CodeBlockOrStmt::end() const {
		if (code_block)
			return code_block.value().internal()->end();
		else if (stmt)
			return { stmt->give(), 1 };
		CORE_UNREACHABLE();
	}
}

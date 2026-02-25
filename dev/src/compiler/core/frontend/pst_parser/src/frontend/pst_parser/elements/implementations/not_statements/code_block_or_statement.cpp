#include "../../hierarchy/not_statements/code_block_or_statement.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<CodeBlockOrStmt> CodeBlockOrStmt::parse(LangParserState& state) {
		auto out = makeBox<CodeBlockOrStmt>(state.getPosition());
		if (state[0].isBracketGroup(Token::BracketType::Curly)) {
			MBox<CodeBlock> block;
			state.parse(out).one(&block);
			if (!block) return nullptr;
			state.parse(out).assign(&out->code_block, std::move(block));
		} else {
			MBox<Stmt> stmt;
			state.parse(out).one(&stmt);
			if (!stmt) return nullptr;
			state.parse(out).assign(&out->stmt, std::move(stmt));
		}

		PST_RETURN out;
	}

	void CodeBlockOrStmt::dprint(std::ostream& out) const {
		if (code_block)
			nullAwareDprint(code_block.value(), out);
		else
			nullAwareDprint(stmt.value(), out);
	}

	LangElement::HashAlg& CodeBlockOrStmt::addElementDataToStableHash(HashAlg& partial_hash) const {
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

	CodeBlockOrStmt::Type CodeBlockOrStmt::getType() const {
		if (stmt.has_value())
			return Type::SingleStmt;
		else if (code_block.has_value())
			return Type::CodeBlock;
		CORE_UNREACHABLE();
	}

	AccessLocked<Stmt> CodeBlockOrStmt::getStmt() const {
		CORE_ASSERT(stmt.has_value(), "No stmt present when getting single statement");
		return stmt.value().give();
	}
}

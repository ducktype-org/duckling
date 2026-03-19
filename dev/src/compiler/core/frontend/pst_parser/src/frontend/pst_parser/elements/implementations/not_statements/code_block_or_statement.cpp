#include "../../hierarchy/not_statements/code_block_or_statement.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<CodeBlockOrStmt> CodeBlockOrStmt::parse(LangParserState& state) {
		auto out = makeBox<CodeBlockOrStmt>(state);
		if (state[0].isBracketGroup(Token::BracketType::Curly)) {
			MBox<CodeBlock> block;
			PARSE().one(&block);
			if (!block) return nullptr;
			PARSE().assign(&out->code_block, std::move(block));
		} else {
			MBox<Stmt> stmt;
			PARSE().one(&stmt);
			if (!stmt) return nullptr;
			PARSE().assign(&out->stmt, std::move(stmt));
		}

		PST_RETURN out;
	}

	void CodeBlockOrStmt::dprint(std::ostream& out) const {
		if (code_block)
			nullAwareDprint(code_block.value(), out);
		else
			nullAwareDprint(stmt.value(), out);
	}

	HashAlg& CodeBlockOrStmt::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, code_block.has_value());
		addToHash(partial_hash, stmt.has_value());
		return partial_hash;
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

	AccessLocked<CodeBlock> CodeBlockOrStmt::getCodeBlock() const {
		CORE_ASSERT(code_block.has_value(), "No code block present when getting code block");
		return code_block.value().give();
	}
}

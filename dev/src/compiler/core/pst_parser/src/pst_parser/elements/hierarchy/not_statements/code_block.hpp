#pragma once

#include "../meta.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Code Block that contains statements.
	 */
	class CodeBlock final: public NotStmt {
		std::vector<AccessInternal<Stmt>> statements;

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(statements, Stmt)

		explicit CodeBlock(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::CodeBlock;
		}

		static MBox<CodeBlock> parse(LangParserState& state);
		~CodeBlock() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Block";
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}

		void acceptVisitor(PstVisitor& visitor) const final;
	};
}

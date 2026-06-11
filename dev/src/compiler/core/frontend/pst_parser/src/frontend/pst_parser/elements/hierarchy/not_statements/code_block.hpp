#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Code Block that contains statements.
	 */
	class CodeBlock final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(CodeBlock, NotStmt, type);
		CLONE_SUBELEMENTS();
	private:
		std::vector<AccessInternalAnonymous<Stmt>> statements;
		BlockOrderType                             type = BlockOrderType::Undefined;

		/**
		 * This is the division of statements inside the block based on their symbol declaration
		 * kind and the symbol they declare, more detailed information about symbol declaration
		 * kinds is in the declaration of Stmt.
		 */
		base::Map<base::StrID, std::vector<AccessLocked<Stmt>>> by_symbol;
		std::vector<AccessLocked<Stmt>>                         no_symbol;
		std::vector<AccessLocked<Stmt>>                         transparent;

		/**
		 * @brief Fills the by_symbol, no_symbol and transparent variables to reflect an ordered
		 * code block.
		 */
		void fillSymbols();

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(statements, Stmt)

		explicit CodeBlock(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::CodeBlock;
		}

		static MBox<CodeBlock> parse(LangParserState& state);
		~CodeBlock() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Block";
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}

		void calcElementPathHashRecursive() override;
	};
}

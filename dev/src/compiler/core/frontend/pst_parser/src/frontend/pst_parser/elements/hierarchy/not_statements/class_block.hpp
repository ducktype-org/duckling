#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Class Block that contains Class statements.
	 */
	class ClassBlock final: public NotStmt {
		std::vector<AccessInternalAnonymous<ClassStmt>> statements;

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
		DECLARE_CONST_ELEMENT_ITERATOR(statements, ClassStmt)

		explicit ClassBlock(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::ClassBlock;
		}

		static MBox<ClassBlock> parse(LangParserState& state);

		~ClassBlock() override = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Block";
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}

		void calcElementPathHashRecursive() override;
	};
}

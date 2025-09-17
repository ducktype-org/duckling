#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Code Block that contains statements.
	 */
	class CodeBlock final: public NotStmt {
	public:
		/**
		 * @brief Type of code block
		 *
		 * Relevant for some behaviors, for example path to node as different blocks can be ordered
		 * like in a function or more unordered like in the global scope. Undefined is just a
		 * default that will cause an error if another type is not set.
		 *
		 * Unordered - Statements that declare the different symbols, statements that don't declare
		 * symbols and transparent statements have separate orders. Ordered - Order of statements is
		 * as one list. Undefined - Illegal default state.
		 */
		enum CodeBlockType {
			Unordered,
			Ordered,
			Undefined,
		};

	private:
		std::vector<AccessInternalAnonymous<Stmt>> statements;
		CodeBlockType                              type = Undefined;

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

		explicit CodeBlock(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::CodeBlock;
		}

		static MBox<CodeBlock> parse(LangParserState& state, CodeBlockType order_type);
		~CodeBlock() final = default;
		void dprint(std::ostream& out) const final;
		u64 calcStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Block";
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}

		void calcElementPathsRecursive() override;
	};
}

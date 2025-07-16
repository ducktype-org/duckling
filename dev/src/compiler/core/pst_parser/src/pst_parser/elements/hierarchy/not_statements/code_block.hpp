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
		 * Relevant for some behaviors, for example path to node as different blocks can be ordered like in a function or more unordered like in the global scope.
		 */
		enum CodeBlockType {
			Unordered, 
			Ordered,
			Undefined,
		};

	private:
		std::vector<AccessInternal<Stmt>> statements;
		CodeBlockType type = Undefined;

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(statements, Stmt)

		explicit CodeBlock(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::CodeBlock;
		}

		static MBox<CodeBlock> parse(LangParserState& state, CodeBlockType order_type);
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
	};
}

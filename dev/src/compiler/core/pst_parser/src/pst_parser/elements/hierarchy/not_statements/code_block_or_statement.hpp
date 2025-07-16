#pragma once

#include "../meta.hpp"
#include "code_block.hpp"

namespace pst {
	/**
	 * @brief Code Block or Statement.
	 */
	class CodeBlockOrStmt final: public NotStmt {
		std::variant<AccessInternal<Stmt>, AccessInternal<CodeBlock>> content;

	public:
		explicit CodeBlockOrStmt(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::CodeBlockOrStmt;
		}

		static MBox<CodeBlockOrStmt> parse(LangParserState& state, CodeBlock::CodeBlockType code_block_order_type);
		~CodeBlockOrStmt() final = default;
		void dprint(std::ostream& out) const final;

		using const_iterator = CodeBlock::const_iterator;
		[[nodiscard]]
		const_iterator begin() const;
		[[nodiscard]]
		const_iterator end() const;

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Block or Statement";
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}
	};
}

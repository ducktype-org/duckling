#pragma once

#include "../meta.hpp"
#include "code_block.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace pst {
	class CodeBlockOrStmtIterator;

	/**
	 * @brief Code Block or Statement.
	 */
	class CodeBlockOrStmt final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(CodeBlockOrStmt, NotStmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD_OPT(stmt, Stmt);
		NAMED_CHILD_OPT(code_block, CodeBlock);

	public:
		enum class Type { SingleStmt, CodeBlock };

		explicit CodeBlockOrStmt(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::CodeBlockOrStmt;
		}

		static MBox<CodeBlockOrStmt> parse(LangParserState& state);
		~CodeBlockOrStmt() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		Type getType() const;

		/**
		 * @brief Get the stored statement. Panics if is in code block state.
		 */
		[[nodiscard]]
		AccessLocked<Stmt> getStmt() const;

		/**
		 * @brief Get the stored code block. Panics if is in stmt state.
		 */
		[[nodiscard]]
		AccessLocked<CodeBlock> getCodeBlock() const;

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

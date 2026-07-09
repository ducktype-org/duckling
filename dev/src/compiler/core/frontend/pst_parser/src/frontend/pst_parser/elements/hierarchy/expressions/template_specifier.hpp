#pragma once

#include "../lists/template_list.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element representing template initialization in an expression. For example:
	 * `list:{i32}`.
	 */
	class TemplateSpecifier final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TemplateSpecifier, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(inner, TemplateList);

	public:
		TemplateSpecifier(const LangParserState& state): ExprElement(state, 300) {
			this->element_kind = ElementKind::ExprElement;
		}

		static MBox<ExprElement> parse(LangParserState& state);

		[[nodiscard]]
		auto getArgumentList() const -> AccessLocked<TemplateList> {
			return inner.give();
		}

		~TemplateSpecifier() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Template Specifier Expression";
		}
	};
}

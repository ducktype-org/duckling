#pragma once

#include "../lists/template_list.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element representing template initialization in an expression. For example:
	 * `list:{i32}`.
	 */
	class TemplateSpecifier final: public ExprElement {
		NAMED_CHILD(inner, TemplateList);

	public:
		TemplateSpecifier(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

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

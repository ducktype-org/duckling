#pragma once

#include "../not_statements/format_string_sub_elements/format_sub_element.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element representing a string value in an expression
	 */
	class ExprFormatStrValue final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ExprFormatStrValue, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		std::vector<AccessInternalAnonymous<FormatSubElement>> sub_elements;

	public:
		explicit ExprFormatStrValue(LangElementConstructionArgument state): ExprElement(state, 0) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~ExprFormatStrValue() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
		void     calcElementPathHashRecursive() override;

		[[nodiscard]]
		auto getSubElements() const {
			using namespace std::views;
			static auto give_one
				= [](const auto& ref) -> AccessLocked<FormatSubElement> { return ref.give(); };
			return std::ranges::ref_view(sub_elements) | transform(give_one);
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Format String Value Expr";
		}
	};
}

#pragma once

#include "../lists/array_literal_list.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Array literal expression in square brackets
	 */
	class ArrayLiteral final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ArrayLiteral, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(list, ArrayLiteralList);

	public:
		explicit ArrayLiteral(const LangParserState& state): ExprElement(state, 200) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~ArrayLiteral() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Array Literal";
		}

		[[nodiscard]]
		AccessLocked<ArrayLiteralList> getList() const {
			return list.give();
		}
	};
}

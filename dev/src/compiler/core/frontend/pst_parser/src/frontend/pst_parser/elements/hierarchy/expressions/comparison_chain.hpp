#pragma once

#include "../not_statements/wrapper_elements/operator_wrapper.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief This class represents a chain of compared expressions for example: `0 < a + b <=
	 * c.size()`
	 *
	 * The chain is stored as a list of sub-expressions and a list of operators between them.
	 */
	class ComparisonChain final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ComparisonChain, ExprElement);
		CLONE_SUBELEMENTS();
	protected:
		using Lower = GeneralBinary;

		std::vector<AccessInternalAnonymous<ExprElement>>     sub_expr;
		std::vector<AccessInternalAnonymous<OperatorWrapper>> operators;

		static i64 skipToOp(const LangParserState& state, i64 base);

	public:
		ComparisonChain(const LangParserState& state): ExprElement(state, 600) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Comparison Chain";
		}

		static MBox<ExprElement> parse(LangParserState& state);

		[[nodiscard]]
		u64 numberOfSubExpressions() const {
			return sub_expr.size();
		}

		[[nodiscard]]
		auto getOperator(u64 index) const {
			return operators.at(index).give();
		}

		[[nodiscard]]
		auto getSubExpr(u64 index) const {
			return sub_expr.at(index).give();
		}

		~ComparisonChain() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		void calcElementPathHashRecursive() override;
	};
}

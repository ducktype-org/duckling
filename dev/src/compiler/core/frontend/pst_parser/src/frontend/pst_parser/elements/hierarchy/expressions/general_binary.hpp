#pragma once

#include "binary_operator.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief General binary operator
	 *
	 * Excludes `and`, `or`
	 */
	class GeneralBinary final: public BinaryOperator {
		using Lower = GeneralSuffix;
		using Self  = GeneralBinary;

		struct OperatorBuilder;

		using BuilderExpr = std::variant<i64, Box<OperatorBuilder>>;

		struct OperatorBuilder final {
			BuilderExpr lhs;
			Operator    type;
			BuilderExpr rhs;
		};

	public:
		explicit GeneralBinary(const LangParserState& state, Operator op):
			  BinaryOperator(state, op.getGenBinOpPrecedence()) {}

		/**
		 * @brief
		 *
		 * @note Assumes an expression atom ends on either:
		 * 1. End of expression
		 * 2. A General binary operator
		 * 3. Literal that isn't following an access operator (`.`, in future also `.?`, maybe
		 * `::`)
		 *
		 */
		static i64 skipAtom(const LangParserState& state, i64 base, i64 length);

		static MBox<ExprElement> parseRecursive(LangParserState& state, const BuilderExpr& expr);

		static MBox<ExprElement> parse(LangParserState& state);

		~GeneralBinary() override = default;
	};
}

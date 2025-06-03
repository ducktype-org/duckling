#pragma once

#include "preamble.hpp"

namespace pst::expr {
	/**
	 * @brief Ternary expression(`if condition then if_true else if_else`).
	 *
	 * @note For now the parsing of ternary is pretty limited with only one such expression
	 * without any parenthesis. This is a limited but safe option.
	 *
	 * @note This should be the default starting level for an expression when comma expression
	 * would cause parsing problems.
	 */
	class Ternary final: public ExprElement {
		using Lower = LogicOr;

		AccessInternal<ExprElement> condition;
		AccessInternal<ExprElement> if_true;
		AccessInternal<ExprElement> if_false;

	public:
		explicit Ternary(const dia::SourcePosition& position): ExprElement(position, 800) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Ternary Expr";
		}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~Ternary() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;

		[[nodiscard]]
		AccessLocked<ExprElement> getCondition() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getIfTrue() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getIfFalse() const;
	};
}

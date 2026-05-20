#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Cast / type assertion: `value as type` (very low precedence).
	 */
	class CastAs final: public ExprElement {
		using Lower = LogicOr;
		using Self  = CastAs;

		NAMED_CHILD(value, ExprElement);
		NAMED_CHILD(type, ExprElement);

	public:
		explicit CastAs(const LangParserState& state): ExprElement(state, 100) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Cast As";
		}

		[[nodiscard]]
		AccessLocked<ExprElement> getValueExpression() const {
			return value.give();
		}

		[[nodiscard]]
		AccessLocked<ExprElement> getTypeExpression() const {
			return type.give();
		}

		static MBox<ExprElement> parse(LangParserState& state);

		~CastAs() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;
	};
}

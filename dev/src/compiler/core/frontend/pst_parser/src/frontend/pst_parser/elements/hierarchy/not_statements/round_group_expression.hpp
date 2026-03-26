#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Expression surrounded by parenthesis.
	 */
	class RoundGroupExpr final: public NotStmt {
		NAMED_CHILD(expr, CommaAllowBlocksExprHolder);

	public:
		explicit RoundGroupExpr(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::RoundGroupExpr;
		}

		static MBox<RoundGroupExpr> parse(LangParserState& state);
		~RoundGroupExpr() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Round Group Expression";
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getExpr() const {
			return expr.give();
		}
	};
}

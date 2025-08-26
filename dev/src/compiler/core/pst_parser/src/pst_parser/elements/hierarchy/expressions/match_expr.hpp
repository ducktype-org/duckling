#pragma once

#include "../not_statements/match_case.hpp"
#include "expr_common.hpp"
#include "pst_parser/access.hpp"

namespace pst::expr {
	/**
	 * @brief Represents the full match expression.
	 * TODOP: Move that to declarations maybe.
	 */
	class MatchExpr final: public ExprElement {
		AccessInternal<CommaExprHolder>        value_to_match;  // TODOP: ???
		std::vector<AccessInternal<MatchCase>> cases;           // TODOP: MatchCaseList?

	public:
		explicit MatchExpr(const dia::SourcePosition& pos): ExprElement(pos, 200) {
			this->element_kind = ElementKind::Match;
		}

		static MBox<MatchExpr> parse(LangParserState& state);

		~MatchExpr() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Match Expr";
		}

		[[nodiscard]] const AccessLocked<CommaExprHolder> getValueToMatch() const {
			return value_to_match.give();
		}

		[[nodiscard]] const std::vector<AccessInternal<MatchCase>>& getCases() const {
			// TODOP: Map with give()
			return cases;
		}
	};
}

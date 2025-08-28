#pragma once

#include "../not_statements/match_case.hpp"
#include "expr_common.hpp"

#include <ranges>

namespace pst::expr {
	/**
	 * @brief Represents the full match expression.
	 */
	class MatchExpr final: public ExprElement {
		AccessInternal<CommaExprHolder>        value_to_match;
		std::vector<AccessInternal<MatchCase>> cases;

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

		[[nodiscard]] auto getCases() const {
			using namespace std::views;
			static auto give_one
				= [](const auto& ref) -> AccessLocked<MatchCase> { return ref.give(); };
			return std::ranges::ref_view(cases) | transform(give_one);
		}
	};
}

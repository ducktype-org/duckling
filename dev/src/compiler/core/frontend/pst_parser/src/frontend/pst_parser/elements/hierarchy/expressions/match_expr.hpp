#pragma once

#include "../not_statements/match_case.hpp"
#include "expr_common.hpp"

#include <ranges>

namespace pst::expr {
	/**
	 * @brief Represents the full match expression.
	 *
	 * It is self delimited (`match ( ... ) { ... }`), so it is parsed as an atom and can appear
	 * anywhere a primary expression can.
	 */
	class MatchExpr final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(MatchExpr, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(value_to_match, CommaExprHolder);
		std::vector<AccessInternalAnonymous<MatchCase>> cases;

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit MatchExpr(const LangParserState& state): ExprElement(state, 200) {
			this->element_kind = ElementKind::Match;
		}

		static MBox<ExprElement> parse(LangParserState& state);

		~MatchExpr() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Match Expr";
		}

		[[nodiscard]] AccessLocked<CommaExprHolder> getValueToMatch() const {
			return value_to_match.give();
		}

		[[nodiscard]] auto getCases() const {
			using namespace std::views;
			static auto give_one
				= [](const auto& ref) -> AccessLocked<MatchCase> { return ref.give(); };
			return std::ranges::ref_view(cases) | transform(give_one);
		}

		void calcElementPathHashRecursive() override;
	};
}

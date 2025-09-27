#pragma once
#include "../meta.hpp"
#include "patterns/flow_pattern.hpp"

#include <ranges>
#include <vector>

namespace pst {
	/**
	 * @brief Single match case for the match expression.
	 */
	class MatchCase final: public NotStmt {
		struct CaseBranch {
			NAMED_CHILD_OPT(condition, UniversalExprHolder);
			NAMED_CHILD(result, UniversalExprHolder);
		};

		struct CaseBranchView {
			base::Optional<AccessLocked<UniversalExprHolder>> condition;
			AccessLocked<UniversalExprHolder>                 result;
		};

		NAMED_CHILD(pattern, FlowPattern);

		/**
		 * @brief Represents all the possible conditions for the case.
		 * For 'cases' with no conditions this vector holds one 'CaseBranch' element with an empty
		 * optional and the result.
		 */
		std::vector<CaseBranch> branches;

	public:
		explicit MatchCase(dia::SourcePosition& pos): NotStmt(pos) {
			this->element_kind = ElementKind::MatchCase;
		}

		static MBox<MatchCase> parse(LangParserState& state);
		~MatchCase() final = default;

		void dprint(std::ostream& out) const final;

		[[nodiscard]] AccessLocked<FlowPattern> getPattern() const { return pattern.give(); }

		[[nodiscard]] std::vector<CaseBranchView> getBranches() const;

		[[nodiscard]]
		std::string elementType() const override {
			return "Match Case";
		}
	};
}

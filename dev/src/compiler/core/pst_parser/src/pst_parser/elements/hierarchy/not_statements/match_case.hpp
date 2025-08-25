#pragma once
#include "../meta.hpp"
#include "patterns.hpp"

namespace pst {
	// TODOP: Maybe put MatchCase and MatchExpr into one file?

	/**
	 * @brief Single match case for the match expression.
	 */
	class MatchCase final: public NotStmt {
		struct CaseBranch {
			base::Optional<AccessInternal<UniversalExprHolder>> condition;
			AccessInternal<AssignmentExprHolder>                result;
		};

		AccessInternal<FlowPattern> pattern;

		// TODOP: Change that to branch list?
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

		[[nodiscard]] const AccessLocked<FlowPattern> getPattern() const { return pattern.give(); }

		[[nodiscard]] const std::vector<CaseBranch>& getBranches() const { return branches; }

		[[nodiscard]]
		std::string elementType() const override {
			return "Match Case";
		}
	};
}

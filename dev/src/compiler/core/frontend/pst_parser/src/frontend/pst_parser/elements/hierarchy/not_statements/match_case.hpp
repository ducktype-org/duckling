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
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(MatchCase, NotStmt);
		CLONE_SUBELEMENTS();

	private:
		struct CaseBranch {
			NAMED_CHILD_OPT(condition, UniversalExprHolder);
			NAMED_CHILD(result, UniversalAllowBlockExprHolder);

			/**
			 * @note This method doesn't add all the data from CaseBranch, just the readable data
			 * without accessing.
			 * It is not consistent with intended behavior of addToHash, but It is really useful
			 * because it lets us use the library defined vector hashing
			 */
			friend constexpr void addToHash(
				hashing::hash_algorithm auto& h, const CaseBranch& t
			) noexcept {
				hashing::addToHash(h, t.condition.has_value());
			}
		};

		struct CaseBranchView {
			base::Optional<AccessLocked<UniversalExprHolder>> condition;
			AccessLocked<UniversalAllowBlockExprHolder>       result;
		};

		NAMED_CHILD(pattern, FlowPattern);

		/**
		 * @brief Represents all the possible conditions for the case.
		 * For 'cases' with no conditions this vector holds one 'CaseBranch' element with an empty
		 * optional and the result.
		 */
		std::vector<CaseBranch> branches;

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit MatchCase(LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::MatchCase;
		}

		static MBox<MatchCase> parse(LangParserState& state);
		~MatchCase() final = default;

		void dprint(std::ostream& out) const final;

		[[nodiscard]] AccessLocked<FlowPattern> getPattern() const { return pattern.give(); }

		[[nodiscard]] auto getBranches() const {
			auto to_branch_view = [](const CaseBranch& internal) -> CaseBranchView {
				return { .condition = internal.condition.has_value()
					                    ? internal.condition->give()
					                    : base::Optional<AccessLocked<UniversalExprHolder>>{},
					     .result    = internal.result.give() };
			};
			return branches | std::views::transform(to_branch_view)
			     | std::ranges::to<std::vector>();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Match Case";
		}
	};
}

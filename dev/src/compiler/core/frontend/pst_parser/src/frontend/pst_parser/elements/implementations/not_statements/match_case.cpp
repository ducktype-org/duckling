#include "../../hierarchy/not_statements/match_case.hpp"

#include "preamble.hpp"

namespace pst {
	void MatchCase::cloneSubElements(const MatchCase& other) {
		for (auto& branch: other.branches) {
			branches.push_back({});
			CloningUtils::clone(*this, branches.back().condition, branch.condition);
			CloningUtils::clone(*this, branches.back().result, branch.result);
		}
		ELEMENT_CLONE_SUB_ELEMENT(pattern);
		ParentClass::cloneSubElements(other);
	}

	MBox<MatchCase> MatchCase::parse(LangParserState& state) {
		auto out = makeBox<MatchCase>(state);

		if (!assertStmtChoice<MatchCase>(state, state[0].is(Keyword::Case))) return nullptr;

		PARSE().all(Keyword::Case, &out->pattern);

		bool if_case = false;
		do {
			MatchCase::CaseBranch current_branch;
			if (PARSE().tryEat(Keyword::If)) {
				PARSE().all(
					&current_branch.condition,
					NamedOperator::Assign,
					&current_branch.result,
					Special::Semicolon
				);
				if_case = true;
			} else if (PARSE().tryEat(NamedOperator::Assign)) {
				if (if_case) {  // Unconditioned branch after a conditioned branch.
					state.logInt(
						makeBox<UnconditionedBranchAfterConditionedError>(state.getPosition())
					);
					return nullptr;
				}

				PARSE().all(&current_branch.result, Special::Semicolon);
				out->branches.push_back(std::move(current_branch));
				break;
			} else {
				break;
			}
			out->branches.push_back(std::move(current_branch));
		}
		PST_WHILE(true);

		if (out->branches.empty()) {  // Empty match expression.
			state.logInt(makeBox<MatchCaseWithNoBodyError>(state.getPosition()));
			return nullptr;
		}
		PST_RETURN out;
	}

	void MatchCase::dprint(std::ostream& out) const {
		out << "{";
		out << R"("node_type": "Match Case",)";
		out << R"("pattern": )";
		nullAwareDprint(pattern, out);
		out << R"(, "branches": [)";

		bool first_branch = true;
		for (const auto& branch: branches) {
			if (!first_branch) out << ", ";
			out << "{";
			if (branch.condition.has_value()) {
				out << R"("condition": )";
				nullAwareDprint(branch.condition.value(), out);
				out << ", ";
			}
			out << R"("result": )";
			nullAwareDprint(branch.result, out);
			out << "}";
			first_branch = false;
		}
		out << "]}";
	}

	HashAlg& MatchCase::addElementDataToStableHash(HashAlg& partial_hash) const {
		hashing::addToHash(partial_hash, branches);
		return partial_hash;
	}
}

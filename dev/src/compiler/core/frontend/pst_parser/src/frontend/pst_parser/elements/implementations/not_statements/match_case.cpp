#include "../../hierarchy/not_statements/match_case.hpp"

#include "preamble.hpp"

namespace pst {
	class MatchCaseWithNoBodyError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Case branch with no body";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		MatchCaseWithNoBodyError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class DoubleDefaultBranchError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Case expression with two default branches";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		DoubleDefaultBranchError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UnconditionedBranchAfterConditionedError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unconditioned case branch after a conditioned branch";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		UnconditionedBranchAfterConditionedError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<MatchCase> MatchCase::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<MatchCase>(position);

		if (!assertStmtChoice<MatchCase>(state, state[0].is(Keyword::Case))) return nullptr;

		state.parse(out).all(Keyword::Case, &out->pattern);

		bool if_case = false;
		do {
			MatchCase::CaseBranch current_branch;
			if (state.parse(out).tryEat(Keyword::If)) {
				state.parse(out).all(
					&current_branch.condition,
					NamedOperator::Assign,
					&current_branch.result,
					Special::Semicolon
				);
				if_case = true;
			} else if (state.parse(out).tryEat(NamedOperator::Assign)) {
				if (if_case) {  // Unconditioned branch after a conditioned branch.
					state.log(makeBox<UnconditionedBranchAfterConditionedError>(state.getPosition())
					);
					return nullptr;
				}

				state.parse(out).all(&current_branch.result, Special::Semicolon);
				out->branches.push_back(std::move(current_branch));
				break;
			} else {
				break;
			}
			out->branches.push_back(std::move(current_branch));
		} PST_WHILE (true);

		if (out->branches.empty()) {  // Empty match expression.
			state.log(makeBox<MatchCaseWithNoBodyError>(state.getPosition()));
			return nullptr;
		}
		return out;
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

	LangElement::HashAlg& MatchCase::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, branches);
		return partial_hash;
	}
}

#include "../../hierarchy/not_statements/stmt_specifier.hpp"

#include "../../hierarchy/lists/call_list.hpp"                         // IWYU pragma: keep
#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	const std::set<Keyword> StmtSpecifier::SPECIFIEIRS_CALL_LIST_REQUIRED(
		SPECIFIEIRS_CALL_LIST_REQUIRED_ARRAY.begin(), SPECIFIEIRS_CALL_LIST_REQUIRED_ARRAY.end()
	);

	MBox<StmtSpecifier> StmtSpecifier::parse(LangParserState& state) {
		auto out = makeBox<StmtSpecifier>(state);

		auto keyword = state[0].asKeyword();
		if (!assertStmtChoice<StmtSpecifier>(
				state,
				lang_def::keywordFlags(keyword).contains(lang_def::KeywordFlagsOptions::IsSpecifier)
			))
			return nullptr;

		if (SPECIFIEIRS_CALL_LIST_REQUIRED.contains(keyword)) {
			PARSE().one(&out->specifier);

			if (!state[0].isBracketGroup(lexer::Token::Round)) {
				state.logSafeError(
					makeBox<NoExternArgumentError>(dia::SourcePosition(state.getPosition()))
				);
				return nullptr;
			}

			PARSE().goDown();
			PARSE().one(&out->call_list);
			PARSE().goUpAndSkip();
		} else {
			PARSE().one(&out->specifier);
		}
		PST_RETURN out;
	}

	void StmtSpecifier::dprint(std::ostream& out) const {
		out << "{";

		out << R"("specifier": )";
		nullAwareDprint(specifier, out);
		out << R"(,)";

		if (call_list) {
			out << R"("call_list": )";
			nullAwareDprint(call_list.value(), out);
		}

		out << "}";
	}

	HashAlg& StmtSpecifier::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}
}

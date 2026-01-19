#include "../../hierarchy/not_statements/stmt_specifier.hpp"

#include "../../hierarchy/lists/call_list.hpp"
#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

#include <diagnostic/message.hpp>

namespace pst {
	const std::set<Keyword> StmtSpecifier::SPECIFIEIRS_CALL_LIST_REQUIRED(
		SPECIFIEIRS_CALL_LIST_REQUIRED_ARRAY.begin(), SPECIFIEIRS_CALL_LIST_REQUIRED_ARRAY.end()
	);

	MBox<StmtSpecifier> StmtSpecifier::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<StmtSpecifier>(position);

		auto keyword = state[0].asKeyword();
		if (!assertStmtChoice<StmtSpecifier>(
				state,
				lang_def::keywordFlags(keyword).contains(lang_def::KeywordFlagsOptions::IsSpecifier)
			))
			return nullptr;

		if (SPECIFIEIRS_CALL_LIST_REQUIRED.contains(keyword)) {
			state.parse(out).one(&out->specifier);

			if (!state[0].isBracketGroup(lexer::Token::Round)) {
				state.logSafeError(
					makeBox<NoExternArgumentError>(dia::SourcePosition(state.getPosition()))
				);
				return out;
			}

			state.parse(out).goDown();
			state.parse(out).one(&out->call_list);
			state.parse(out).goUpAndSkip();
		} else {
			state.parse(out).one(&out->specifier);
		}
		return out;
	}

	void StmtSpecifier::dprint(std::ostream& out) const {
		out << "{";

		out << strConcat(R"("specifier": ")", lang_def::keywordToStr(specifier), R"(",)");

		if (call_list) {
			out << R"("call_list": )";
			nullAwareDprint(call_list.value(), out);
			out << ",";
		}

		out << "}";
	}

	LangElement::HashAlg& StmtSpecifier::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, lang_def::keywordToStr(specifier));
		return partial_hash;
	}
}

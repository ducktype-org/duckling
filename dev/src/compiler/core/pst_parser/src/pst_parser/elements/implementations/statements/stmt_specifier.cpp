#include "../../hierarchy/statements/stmt_specifier.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	const std::set<Keyword> StmtSpecifier::SPECIFIERS(
		SPECIFIERS_ARRAY.begin(), SPECIFIERS_ARRAY.end()
	);

	MBox<StmtSpecifier> StmtSpecifier::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<StmtSpecifier>(position);

		out->specifier = state[0].asKeyword();
		if (!assertStmtChoice<StmtSpecifier>(state, SPECIFIERS.contains(out->specifier)))
			return nullptr;

		state.parse(out).all(out->specifier, &out->code_block_or_stmt);

		return out;
	}

	void StmtSpecifier::dprint(std::ostream& out) const {
		out << "{";

		out << strConcat(R"("specifier": ")", lang_def::keywordToStr(specifier), R"(",)");

		out << R"("stmt": )";

		nullAwareDprint(code_block_or_stmt, out);

		out << "}";
	}

	bool StmtSpecifier::trailingSemicolon() { return false; }

	void StmtSpecifier::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitStmtSpecifier(*this);
	}
}

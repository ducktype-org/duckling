#include "../../hierarchy/statements/stmt_specifier.hpp"

#include "../../hierarchy/lists/call_list.hpp"
#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {

	const std::set<Keyword> StmtSpecifier::SPECIFIERS(
		SPECIFIERS_ARRAY.begin(), SPECIFIERS_ARRAY.end()
	);

	const std::set<Keyword> StmtSpecifier::SPECIFIEIRS_CALL_LIST_REQUIRED(
		SPECIFIEIRS_CALL_LIST_REQUIRED_ARRAY.begin(), SPECIFIEIRS_CALL_LIST_REQUIRED_ARRAY.end()
	);

	MBox<StmtSpecifier> StmtSpecifier::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<StmtSpecifier>(position);

		out->specifier = state[0].asKeyword();
		if (!assertStmtChoice<StmtSpecifier>(state, SPECIFIERS.contains(out->specifier)))
			return nullptr;

		if (SPECIFIEIRS_CALL_LIST_REQUIRED.contains(out->specifier)) {
			state.parse(out).one(out->specifier);
			MBox<CallList> call_list;

			if (!state[0].isBracketGroup(lexer::Token::Round)) {
				state.logInt(makeBox<BadSpecifierCallError>(dia::SourcePosition(state.getPosition())));
				return nullptr;
			}

			state.parse(out).goDown();
			state.parse(out).one(&call_list);

			if (!call_list) return nullptr;
			state.parse(out).goUpAndSkip();

			state.parse(out).assign(&out->call_list, std::move(call_list));
			state.parse(out).withDef(&out->code_block_or_stmt, CodeBlock::CodeBlockType::Unordered);
		} else {
			state.parse(out)
				.one(out->specifier)
				.withDef(&out->code_block_or_stmt, CodeBlock::CodeBlockType::Unordered);
		}

		// @TODO: #1739 Move this handling outside of parser.
		// Check if only legal elements are present in the extern block or statement
		// (for example "print();" is illegal, only function and classes are allowed)
		if (out->specifier == Keyword::Extern) {
			for (auto&& stmt: *out->code_block_or_stmt.internal()) {
				auto kind = stmt.illegalAccess().value()->getStmtKind();
				if (kind != StmtKind::Fun && kind != StmtKind::Class && kind != StmtKind::FunDecl) {
					state.logInt(makeBox<InvalidExternContentWarning>(
						stmt.illegalAccess().value()->getSourcePosition()
					));
				}
			}
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

		out << R"("stmt": )";

		nullAwareDprint(code_block_or_stmt, out);

		out << "}";
	}

	LangElement::HashAlg& StmtSpecifier::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, lang_def::keywordToStr(specifier));
		return partial_hash;
	}

	bool StmtSpecifier::trailingSemicolon() { return false; }

	void StmtSpecifier::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitStmtSpecifier(*this);
	}
}

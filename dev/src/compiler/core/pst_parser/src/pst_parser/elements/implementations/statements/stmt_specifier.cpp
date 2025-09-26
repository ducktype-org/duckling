#include "../../hierarchy/statements/stmt_specifier.hpp"

#include "../../hierarchy/lists/call_list.hpp"
#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	class BadCallError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected a round bracket call expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadCallError(dia::SourcePosition pos): dia::Error(pos) {}
	};

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
				state.log(makeBox<BadCallError>(dia::SourcePosition(state.getPosition())));
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

	bool StmtSpecifier::trailingSemicolon() { return false; }

	void StmtSpecifier::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitStmtSpecifier(*this);
	}
}

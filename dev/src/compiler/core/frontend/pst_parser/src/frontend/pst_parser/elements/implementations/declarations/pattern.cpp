#include "../../hierarchy/declarations/pattern.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	class PatternArgumentCountError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Pattern declarations must have exactly one parameter.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		PatternArgumentCountError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class PatternBracketError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected round bracket group.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		PatternBracketError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<Pattern> Pattern::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Pattern>(position);

		if (!assertStmtChoice<Pattern>(state, state[0].is(Keyword::Pattern))) return nullptr;

		state.parse(out).all(Keyword::Pattern, &out->name);

		// Patterns take in only one argument, thus we don't use the parametr list and check for
		// braces manually.
		if (!state[0].isBracketGroup(Token::Round)) {
			state.log(makeBox<PatternBracketError>(state.getPosition()));
			return nullptr;
		}

		auto bracket_group_token = state[0];
		state.parse(out).goDown();
		if (state.empty()) {
			state.log(makeBox<PatternArgumentCountError>(bracket_group_token.getPosition()));
			return nullptr;
		}

		state.parse(out).one(&out->param);
		if (!state.empty()) {  // More than one argument.
			state.log(makeBox<PatternArgumentCountError>(bracket_group_token.getPosition()));
			return nullptr;
		}

		state.parse(out).goUpAndSkip();

		if (state.parse(out).tryEat(NamedOperator::SingleArrow)) state.parse(out).one(&out->ret);

		state.parse(out)
			.one(NamedOperator::Assign)
			.withDef(&out->body, CodeBlock::CodeBlockType::Ordered);

		return out;
	}

	void Pattern::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		nullAwareDprint(name, out);
		out << ",\"parameter\":";
		nullAwareDprint(param, out);
		out << ",\"return\":";
		if (ret)
			nullAwareDprint(ret.value(), out);
		else
			out << "\"unit\"";
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	LangElement::HashAlg& Pattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, name);
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void Pattern::acceptVisitor(PstVisitor& visitor) const { visitor.visitPattern(*this); }
}

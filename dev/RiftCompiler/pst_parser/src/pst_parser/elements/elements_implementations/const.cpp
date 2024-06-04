#include "elements_implementation.hpp"

namespace pst {
	class ConstTypeEndError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected type expression ending with `=`.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		ConstTypeEndError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	ParserRef<Const> Const::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Const>(position);
		RIFT_ASSERT(state[0].is(Keyword::Const), position.genStr("bad statement choice"));

		out->addKeyword(state.getPosition());

		constexpr auto isTypeEnd = [](const RiftParserState& st, i64 fwd) {
			return st[fwd].is(Operator::Assign) || st[fwd].is(Special::Semicolon);
		};

		constexpr auto isAssign
			= [](const RiftParserState& st, i64 fwd) { return st[fwd].is(Operator::Assign); };

		parseAll(state, Keyword::Const, &out->name, Operator::Colon);
		out->type = Expr::parseUntil<isTypeEnd, isAssign, ConstTypeEndError>(state);

		state.tryEat(Operator::Assign);

		parseOne(state, &out->value, true);

		out->setLastToken(state.getPosition(-1));

		return out;
	}

	void Const::dprint(std::ostream& out) const {
		out << "{\"Const\": {";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);
		out << "}}";
	}
}

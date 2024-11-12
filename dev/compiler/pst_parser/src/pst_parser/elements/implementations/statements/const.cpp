#include "preamble.hpp"

namespace pst {
	class ConstTypeEndError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected type expression followed by `=`.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		ConstTypeEndError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	ParserRef<Const> Const::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Const>(position);

		if (!assertStmtChoice<Const>(state, state[0].is(Keyword::Const))) return nullptr;

		std::cerr << state[0].getStrValue() << '\n';
		std::cerr << state[1].getStrValue() << '\n';
		std::cerr << state[2].getStrValue() << '\n';
		state.parse(out).all(Keyword::Const, &out->name, NamedOperator::Colon);

		std::cerr << state[0].getStrValue() << '\n';

		state.parse(out).with(
			&out->type,
			CommaExpr::parse
		);

		// If there is no = then expr parsing already threw an error
		state.parse(out).tryEat(NamedOperator::Assign);

		state.parse(out).with(&out->value, CommaExpr::parse);
		return out;
	}

	void Const::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);
		out << R"(, "value": )";
		nullAwareDprint(value, out);
		out << "}";
	}

	void Const::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitConst(*this); }
}

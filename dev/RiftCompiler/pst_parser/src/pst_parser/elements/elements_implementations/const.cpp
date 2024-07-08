#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

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

	ParserRef<Const> Const::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Const>(position);

		if (!assertStmtChoice<Const>(state, state[0].is(Keyword::Const))) return nullptr;


		state.parse().all(Keyword::Const, &out->name, Operator::Colon);

		out->type = Expr::parseUntil<
			detail::Conditions::isAssignOrSemicolon,
			detail::Conditions::isAssign,
			ConstTypeEndError>(state, true);
		out->addChild(out->type);

		if (state.tryEat(Operator::Assign)) {
			out->addToken(state[-1]);
		}

		out->value = Expr::parse(state, true);

		out->setLastToken(state.getPosition(-1));

		return out;
	}

	void Const::dprint(std::ostream& out) const {
		out << "{\"Const\": {";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);
		out << R"(, "value": )";
		nullAwareDprint(value, out);
		out << "}}";
	}

	void Const::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitConst(*this); }
}

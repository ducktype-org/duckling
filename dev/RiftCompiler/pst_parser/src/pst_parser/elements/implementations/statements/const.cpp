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

	ParserRef<Const> Const::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Const>(position);

		if (!assertStmtChoice<Const>(state, state[0].is(Keyword::Const))) return nullptr;

		state.parse(out).all(Keyword::Const, &out->name, Operator::Colon);

		state.parse(out).with<Expr>(
			&out->type,
			Expr::parseUntil<
				detail::Conditions::isAssignOrSemicolon,
				detail::Conditions::isAssign,
				ConstTypeEndError>,
			true
		);

		// If there is no = then expr parsing already threw an error
		state.parse(out).tryEat(Operator::Assign);

		state.parse(out).with<Expr>(&out->value, Expr::parse, true);
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

	void Const::semPrint(std::ostream& out) const {
		out << "{\"Const\": {";
		getSourcePosition().semPrint(out);
		out << R"(,"semanticTokenType": "variable",)";  // @TODO: maybe needs a change?
		out << R"("name": )";
		nullAwareSemanticTokenPrint(name, out);
		out << R"(, "type": )";
		nullAwareSemanticTokenPrint(type, out);
		out << R"(, "value": )";
		nullAwareSemanticTokenPrint(value, out);
		out << "}}";
	}

	void Const::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitConst(*this); }
}

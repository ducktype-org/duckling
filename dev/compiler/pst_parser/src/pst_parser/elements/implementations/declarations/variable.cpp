#include "preamble.hpp"

namespace pst {
	class VariableTypeEndError final: public dia::Error {
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

		VariableTypeEndError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	ParserRef<Variable> Variable::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Variable>(position);

		const bool is_var = state[0].is(Keyword::Var);
		const bool is_let = state[0].is(Keyword::Let);

		if (!assertStmtChoice<Namespace>(state, is_var || is_let)) return nullptr;

		out->is_const = is_let;

		// @TODO: Add a possibility for type deduction from assigned value and no initial value.
		state.parse(out).all(is_var ? Keyword::Var : Keyword::Let, &out->name, NamedOperator::Colon);

		state.parse(out).with(
			&out->type,
			Expr::parseUntil<
				detail::Conditions::isAssignOrSemicolon,
				detail::Conditions::isAssign,
				VariableTypeEndError>,
			true
		);

		state.parse(out).one(NamedOperator::Assign, true);

		// @TODO: Perhaps add possibility for default construction.
		state.parse(out).with<Expr>(&out->value, Expr::parse, true);

		return out;
	}

	void Variable::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);

		out << "}";
	}

	void Variable::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitVariable(*this); }

	bool Variable::trailingSemicolon() { return true; }
}

#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

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

	ParserRef<Variable> Variable::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Variable>(position);

		const bool is_var = state[0].is(Keyword::Var);
		const bool is_let = state[0].is(Keyword::Let);

		if (!assertStmtChoice<Namespace>(state, is_var || is_let)) return nullptr;

		out->addKeyword(position);
		out->is_const = is_let;

		// @TODO: Add a possibility for type deduction from assigned value and no initial value.
		parseAll(state, is_var ? Keyword::Var : Keyword::Let, &out->name, Operator::Colon);
		out->type = Expr::parseUntil<
			detail::Conditions::isAssignOrSemicolon,
			detail::Conditions::isAssign,
			VariableTypeEndError>(state, true);

		parseOne(state, Operator::Assign, true);
		out->value = Expr::parse(state, true);

		out->setLastToken(state.getPosition());

		return out;
	}

	void Variable::dprint(std::ostream& out) const {
		out << "{\"";
		out << (is_const ? "let" : "var");
		out << "\": {";
		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);
		out << "}}";
	}

	void Variable::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitVariable(*this); }

	bool Variable::trailingSemicolon() { return true; }
}

#include "../../hierarchy/declarations/variable.hpp"

#include "../../hierarchy/expr_holders.hpp"  // IWYU pragma: keep
#include "preamble.hpp"
#include "var_parse.hpp"

namespace pst {
	MBox<Variable> Variable::parse(LangParserState& state) {
		MBox<Variable> out;

		const bool is_var = state[0].is(Keyword::Var);
		const bool is_let = state[0].is(Keyword::Let);

		if (is_var)
			out = parseVariableTemplate<Variable, Keyword::Var>(state);
		else
			out = parseVariableTemplate<Variable, Keyword::Let>(state);
		if (is_let) out->is_const = true;
		return out;
	}

	void Variable::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(, "mutable": )";
		if (is_const)
			out << "false";
		else
			out << "true";
		if (type) {
			out << R"(, "type": )";
			nullAwareDprint(type.value(), out);
		}
		if (value) {
			out << R"(, "value": )";
			nullAwareDprint(value.value(), out);
		}

		out << "}";
	}

	void Variable::acceptVisitor(PstVisitor& visitor) const { visitor.visitVariable(*this); }

	bool Variable::trailingSemicolon() { return true; }
}

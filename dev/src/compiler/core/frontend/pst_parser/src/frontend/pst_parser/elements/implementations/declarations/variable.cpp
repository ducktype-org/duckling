#include "../../hierarchy/declarations/variable.hpp"

#include "../../hierarchy/expr_holders.hpp"  // IWYU pragma: keep
#include "preamble.hpp"
#include "var_parse.hpp"

namespace pst {
	MBox<Variable> Variable::parse(LangParserState& state) {
		MBox<Variable> out;

		if (state[0].is(Keyword::Var))
			out = parseVariableTemplate<Variable, Keyword::Var>(state);
		else
			out = parseVariableTemplate<Variable, Keyword::Let>(state);

		PST_RETURN out;
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

	HashAlg& Variable::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, type.has_value());
		addToHash(partial_hash, value.has_value());
		addToHash(partial_hash, is_const);
		return partial_hash;
	}

	void Variable::acceptVisitor(PstVisitor& visitor) const { visitor.visitVariable(*this); }

	bool Variable::trailingSemicolon() { return true; }
}

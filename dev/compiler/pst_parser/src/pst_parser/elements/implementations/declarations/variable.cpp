#include "preamble.hpp"
#include "var_parse.hpp"

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

	MBox<Variable> Variable::parse(LangParserState& state) {
		MBox<Variable> out;

		const bool is_var = state[0].is(Keyword::Var);
		const bool is_let = state[0].is(Keyword::Let);

		if (is_var)
			out = parseVariableTemplate<Variable, Keyword::Var>(state);
		else
			out = parseVariableTemplate<Variable, Keyword::Let>(state);

		out->is_const = is_let;
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

	void Variable::acceptVisitor(PstVisitor& visitor) const { visitor.visitVariable(*this); }

	bool Variable::trailingSemicolon() { return true; }
}

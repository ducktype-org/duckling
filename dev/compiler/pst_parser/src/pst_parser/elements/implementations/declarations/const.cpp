#include "preamble.hpp"
#include "var_parse.hpp"

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

	MBox<Const> Const::parse(LangParserState& state) {
		return parseVariableTemplate<Const, Keyword::Const>(state);
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

	void Const::acceptVisitor(PstVisitor& visitor) const { visitor.visitConst(*this); }

	bool Const::trailingSemicolon() { return true; }
}

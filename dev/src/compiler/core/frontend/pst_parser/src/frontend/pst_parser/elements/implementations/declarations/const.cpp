#include "../../hierarchy/declarations/const.hpp"

#include "preamble.hpp"
#include "var_parse.hpp"

namespace pst {
	MBox<Const> Const::parse(LangParserState& state) {
		return parseVariableTemplate<Const, Keyword::Const>(state);
	}

	void Const::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);
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

	HashAlg& Const::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, type.has_value());
		addToHash(partial_hash, value.has_value());
		return partial_hash;
	}

	void Const::acceptVisitor(PstVisitor& visitor) const { visitor.visitConst(*this); }

	bool Const::trailingSemicolon() { return true; }
}

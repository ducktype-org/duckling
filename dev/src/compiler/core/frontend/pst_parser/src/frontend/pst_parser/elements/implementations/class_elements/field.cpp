#include "../../hierarchy/class_elements/field.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Field, name, type, init);

	MBox<Field> Field::parse(LangParserState& state) {
		auto out = makeBox<Field>(state);

		if (state[0].is(Keyword::Let)) {
			out->is_mutable = false;
			PARSE().one(Keyword::Let);
		} else
			PARSE().tryEat(Keyword::Var);

		PARSE().all(&out->name, NamedOperator::Colon);
		PARSE().one(&out->type);

		if (PARSE().tryEat(NamedOperator::Assign)) PARSE().one(&out->init);

		PST_RETURN out;
	}

	void Field::dprint(std::ostream& out) const {
		out << "{";

		out << R"("is mutable": )";
		if (is_mutable)
			out << R"("true")";
		else
			out << R"("false")";

		out << R"(,"name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);

		if (init) {
			out << R"(, "initial": )";
			nullAwareDprint(init.value(), out);
		}

		out << "}";
	}

	HashAlg& Field::addElementDataToStableHash(HashAlg& partial_hash) const {
		hashing::addToHash(partial_hash, is_mutable);
		return partial_hash;
	}

	void Field::acceptVisitor(PstVisitor& visitor) const { visitor.visitField(*this); }
}

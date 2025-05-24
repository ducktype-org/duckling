#include "../../hierarchy/class_elements/field.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<Field> Field::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<Field>(position, ctx);

		out->parseSpecifiers(state);

		if (state.parse(out).tryEat(Keyword::Const)) {
			out->is_mutable = false;
			state.parse(out).eatOne();
		}

		state.parse(out).all(&out->name, NamedOperator::Colon);
		state.parse(out).one(&out->type);

		if (state.parse(out).tryEat(NamedOperator::Assign)) state.parse(out).one(&out->init);

		return out;
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

	void Field::acceptVisitor(PstVisitor& visitor) const { visitor.visitField(*this); }
}

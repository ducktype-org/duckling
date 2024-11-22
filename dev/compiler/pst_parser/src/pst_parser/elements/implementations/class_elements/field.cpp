#include "preamble.hpp"

namespace pst {
	class FieldTypeEndError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected type expression followed by `=` or `;`.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		FieldTypeEndError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	ParserRef<Field> Field::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeRef<Field>(position, ctx);

		out->parseSpecifiers(state);

		if (state.parse(out).tryEat(Keyword::Const)) {
			out->is_mutable = false;
			state.parse(out).eatOne();
		}

		state.parse(out).all(&out->name, NamedOperator::Colon);
		state.parse(out).with(&out->type, CommaExpr::parse);

		if (state.parse(out).tryEat(NamedOperator::Assign))
			state.parse(out).with(&out->init, CommaExpr::parse);

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

	void Field::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitField(*this); }
}

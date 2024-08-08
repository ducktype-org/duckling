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

	ParserRef<Field> Field::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Field>(position);

		out->parseSpecifiers(state);

		if (state.parse(out).tryEat(Keyword::Const)) {
			out->is_const = true;
			state.parse(out).eatOne();
		}

		state.parse(out).all(&out->name, Operator::Colon);
		state.parse(out).with(
			&out->type,
			Expr::parseUntil<
				detail::Conditions::isAssignOrSemicolon,
				detail::Conditions::isAssignOrSemicolon,
				FieldTypeEndError>,
			true
		);

		if (state.parse(out).tryEat(Operator::Assign)) {
			state.parse(out).with<Expr>(&out->init, Expr::parse, true);
		}

		return out;
	}

	void Field::dprint(std::ostream& out) const {
		out << "{";

		out << R"("is const": )";
		if(is_const) out << R"("true")";
		else out << R"("false")";

		out << R"(,"name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);

		out << "}";
	}

	void Field::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitField(*this); }
}

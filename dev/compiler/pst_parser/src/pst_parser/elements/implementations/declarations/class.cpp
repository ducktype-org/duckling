#include "preamble.hpp"

namespace pst {
	class ClassEndingError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unexpected end to class definition.";
		}

	public:
		ClassEndingError(dia::SourcePosition pos): dia::Error(pos) {}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}
	};

	ParserRef<Class> Class::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Class>(position);

		if (!assertStmtChoice<Class>(state, state[0].is(Keyword::Class))) return nullptr;

		state.parse(out).all(Keyword::Class, &out->name);

		if (state.parse(out).tryEat(Keyword::Extends)) {
			state.parse(out).with<Expr>(
				&out->base,
				Expr::parseUntil<
					detail::Conditions::isImplementsOrBlockGroup,
					detail::Conditions::isImplementsOrBlockGroup,
					ClassEndingError>,
				false
			);
		}
		if (state.parse(out).tryEat(Keyword::Implements))
			state.parse(out).one(&out->implements, true);

		state.parse(out).with(&out->body, ClassBlock::parse, { out->name, {} });

		return out;
	}

	void Class::dprint(std::ostream& out) const {
		out << R"({"name":)";
		nullAwareDprint(name, out);

		out << R"(,"Base":)";
		nullAwareDprint(base, out);

		out << R"(,"Implements":)";
		nullAwareDprint(implements, out);

		out << R"(,"body": )";
		nullAwareDprint(body, out);

		out << "}";
	}

	void Class::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitClass(*this); }
}

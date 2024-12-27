#include "preamble.hpp"

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
		auto position = state.getPosition();
		auto out      = makeBox<Const>(position);

		if (!assertStmtChoice<Const>(state, state[0].is(Keyword::Const))) return nullptr;

		state.parse(out).all(Keyword::Const, &out->name, NamedOperator::Colon);

		state.parse(out).with(&out->type, CommaExpr::parse);

		state.parse(out).one(NamedOperator::Assign, true);

		state.parse(out).with(&out->value, CommaExpr::parse);
		return out;
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
}

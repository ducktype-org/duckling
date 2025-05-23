#include "../../hierarchy/declarations/class.hpp"

#include "../../hierarchy/expr.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Expr parser for the extends class expression.
	 */
	class ClassExtendsExpr: public NotStmt {
	public:
		static bool end(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Semicolon) || state[fwd].is(NamedOperator::Assign)
			    || detail::Conditions::isImplementsOrBlockGroup(state, fwd);
		}

		static MBox<ExprElement> parse(LangParserState& state) {
			return expr::parseUntil<expr::ChainExpr, end>(state);
		}

		ClassExtendsExpr() = delete;
	};

	MBox<Class> Class::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Class>(position);

		if (!assertStmtChoice<Class>(state, state[0].is(Keyword::Class))) return nullptr;

		state.parse(out).all(Keyword::Class, &out->name);

		if (state.parse(out).tryEat(Keyword::Extends))
			state.parse(out).with(&out->base, ClassExtendsExpr::parse);
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

	void Class::acceptVisitor(PstVisitor& visitor) const { visitor.visitClass(*this); }
}

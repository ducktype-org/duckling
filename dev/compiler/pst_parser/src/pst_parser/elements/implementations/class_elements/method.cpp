#include "preamble.hpp"
#include "../../hierarchy/lists.hpp"  // IWYU pragma: keep

namespace pst {
	namespace {
		class MethodReturnTypeListEndError final: public dia::Error {
		protected:
			[[nodiscard]]
			std::string toStringBrief() const override {
				return "Unexpected end of method return type expression.";
			}

		public:
			[[nodiscard]]
			Domain getDomain() const override {
				return Domain::Parser;
			}

			MethodReturnTypeListEndError(dia::SourcePosition pos): dia::Error(pos) {}
		};
	}

	ParserRef<Method> Method::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeRef<Method>(position, ctx);

		out->parseSpecifiers(state);

		if (!assertStmtChoice<Fun>(state, state[0].is(Keyword::Fun))) return nullptr;

		state.parse(out).all(Keyword::Fun, &out->name, &out->params);
		if (state.parse(out).tryEat(NamedOperator::SingleArrow))
			state.parse(out).with<Expr>(
				&out->ret,
				Expr::parseUntil<
					detail::Conditions::isAssign,
					detail::Conditions::isAssign,
					MethodReturnTypeListEndError>,
				true
			);

		state.parse(out).all(NamedOperator::Assign, &out->body);

		return out;
	}

	void Method::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		nullAwareDprint(name, out);
		out << ",\"parameters\":";
		nullAwareDprint(params, out);
		out << ",\"return\":";
		if (ret)
			nullAwareDprint(ret.value(), out);
		else
			out << "\"unit\"";
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void Method::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitMethod(*this); }
}

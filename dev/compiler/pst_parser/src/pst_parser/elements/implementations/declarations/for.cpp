#include "preamble.hpp"

namespace pst {
	class ForBracketError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected round bracket group.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		ForBracketError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class ForNoInError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected keyword `in` before the end of bracket group.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		ForNoInError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	ParserRef<For> For::parse(LangParserState& state) {
		// @TODO: attr list
		auto position = state.getPosition();
		auto out      = makeRef<For>(position);

		if (!assertStmtChoice<For>(state, state[0].is(Keyword::For))) return nullptr;

		state.parse(out).all(Keyword::For, &out->optional_name);

		if (!state[0].isBracketGroup(Token::Round)) {
			state.log(base::make_unique<ForBracketError>(state.getPosition()));
		} else {
			state.parse(out).goDown();

			state.parse(out).one(&out->iterator, true);

			if (state.parse(out).tryEat(NamedOperator::Colon)) {
				state.parse(out).with<Expr>(
					&out->type,
					Expr::parseUntil<
						detail::Conditions::is<Keyword::In>,
						detail::Conditions::is<Keyword::In>,
						ForNoInError>,
					true
				);
				state.parse(out).tryEat(Keyword::In);
			} else {
				state.parse(out).one(Keyword::In);
			}

			state.parse(out).with<Expr>(&out->iterable, Expr::parse, true);

			state.parse(out).goUpAndSkip();
		}

		state.parse(out).one(&out->body, true);

		return out;
	}

	void For::dprint(std::ostream& out) const {
		out << "{";
		out << R"("name":)";
		nullAwareDprint(optional_name, out);
		out << R"(, "identifier": )";
		nullAwareDprint(iterator, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);
		out << R"(, "iterable": )";
		nullAwareDprint(iterable, out);
		out << R"(, "body": )";
		nullAwareDprint(body, out);
		out << "}";
	}

	void For::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitFor(*this); }
}

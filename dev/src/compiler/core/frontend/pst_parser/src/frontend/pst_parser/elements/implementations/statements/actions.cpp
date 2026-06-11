#include "../../hierarchy/actions/all_actions.hpp"
#include "../../hierarchy/expr_holders.hpp"
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Action, expr);

	MBox<Action> Action::parse(LangParserState& state) {
		if (!assertStmtChoice<Action>(state, state[0].isKeyword())) return nullptr;

		MBox<Action> out;
		auto         keyword = state[0].asKeyword();

		switch (keyword) {
		case Keyword::Return:
			out = makeBox<Return>(state);
			break;
		case Keyword::Break:
			out = makeBox<Break>(state);
			break;
		case Keyword::Continue:
			out = makeBox<Continue>(state);
			break;
		case Keyword::Redo:
			out = makeBox<Redo>(state);
			break;
		case Keyword::Restart:
			out = makeBox<Restart>(state);
			break;
		case Keyword::Defer:
			out = makeBox<Defer>(state);
			break;
		case Keyword::Throw:
			out = makeBox<Throw>(state);
			break;
		default:
			assertStmtChoice<Action>(state, false);
			return nullptr;
		}
		state.parse(out.toOpt().value()).eatOne();

		// @TODO: for now we assume if there is no expression there is a semicolon
		// @TODO: #1535 Change to not parsing expression when no tokens are left
		if (!state[0].is(Special::Semicolon)) state.parse(out.toOpt().value()).one(&out->expr);

		PST_RETURN out;
	}

	base::Optional<AccessLocked<ExprHolder>> Action::getValue() const {
		return expr.map([](const auto& e) -> AccessLocked<ExprHolder> { return e.give(); });
	}

	namespace internal {
		template<base::TemplateStringLiteral name>
		void simpleActionDprint(
			std::ostream&                                                out,
			const std::string&                                           kind,
			const base::Optional<AccessInternal<CommaExprHolder, name>>* expr
		) {
			out << "{";
			out << R"("kind": ")" << kind << "\"";

			if (*expr) {
				out << R"(, "value":)";
				nullAwareDprint(expr->value(), out);
			}
			out << "}";
		}
	}

	HashAlg& Action::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, expr.has_value());
		return partial_hash;
	}

	void Return::dprint(std::ostream& out) const {
		internal::simpleActionDprint(out, "Return", &expr);
	}

	void Break::dprint(std::ostream& out) const {
		internal::simpleActionDprint(out, "Break", &expr);
	}

	void Continue::dprint(std::ostream& out) const {
		internal::simpleActionDprint(out, "Continue", &expr);
	}

	void Redo::dprint(std::ostream& out) const { internal::simpleActionDprint(out, "Redo", &expr); }

	void Restart::dprint(std::ostream& out) const {
		internal::simpleActionDprint(out, "Restart", &expr);
	}

	void Defer::dprint(std::ostream& out) const {
		internal::simpleActionDprint(out, "Defer", &expr);
	}

	void Throw::dprint(std::ostream& out) const {
		internal::simpleActionDprint(out, "Throw", &expr);
	}

	void Return::acceptVisitor(PstVisitor& visitor) const { visitor.visitReturn(*this); }

	void Break::acceptVisitor(PstVisitor& visitor) const { visitor.visitBreak(*this); }

	void Continue::acceptVisitor(PstVisitor& visitor) const { visitor.visitContinue(*this); }

	void Redo::acceptVisitor(PstVisitor& visitor) const { visitor.visitRedo(*this); }

	void Restart::acceptVisitor(PstVisitor& visitor) const { visitor.visitRestart(*this); }

	void Defer::acceptVisitor(PstVisitor& visitor) const { visitor.visitDefer(*this); }

	void Throw::acceptVisitor(PstVisitor& visitor) const { visitor.visitThrow(*this); }
}

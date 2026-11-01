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

		if (keyword == Keyword::Break || keyword == Keyword::Continue) {
			if (state.notEmpty() && !state[0].is(Special::Semicolon)) {
				if (state[0].isKeyword() && isControlFlowTargetKeyword(state[0].asKeyword())) {
					out->target_keyword = state[0].asKeyword();
					state.parse(out.toOpt().value()).eatOne();
				} else if (state[0].isIdentifier()) {
					// A target name is a single identifier, not something complicated.
					state.parse(out.toOpt().value()).autoFallbackLen(u64{ 1 }).one(&out->expr);
				} else {
					state.logInt(makeBox<InvalidControlFlowTargetError>(state.getPosition()));
					PST_RETURN out;
				}

				if (state.notEmpty() && !state[0].is(Special::Semicolon)) {
					if (state[0].isBracketGroup(Token::Square)
					    || state[0].isBracketGroup(Token::Round)) {
						state.logInt(makeBox<ControlFlowTargetArgumentsError>(state.getPosition()));
					} else {
						state.logInt(
							makeBox<UnexpectedControlFlowTargetTokensError>(state.getPosition())
						);
					}
				}
			}
			PST_RETURN out;
		}

		if (state.notEmpty() && !state[0].is(Special::Semicolon))
			state.parse(out.toOpt().value()).one(&out->expr);

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
			const base::Optional<AccessInternal<CommaExprHolder, name>>* expr,
			base::Optional<lang_def::Keyword>                            target_keyword = {}
		) {
			out << "{";
			out << R"("kind": ")" << kind << "\"";

			if (*expr) {
				out << R"(, "value":)";
				nullAwareDprint(expr->value(), out);
			}
			if (target_keyword.has_value())
				out << R"(, "target_keyword": ")"
					<< lang_def::keywordToStr(target_keyword.value()).strView() << '"';
			out << "}";
		}
	}

	HashAlg& Action::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, expr.has_value());
		addToHash(partial_hash, target_keyword.has_value());
		if (target_keyword.has_value()) addToHash(partial_hash, target_keyword.value());
		return partial_hash;
	}

	void Return::dprint(std::ostream& out) const {
		internal::simpleActionDprint(out, "Return", &expr);
	}

	void Break::dprint(std::ostream& out) const {
		internal::simpleActionDprint(out, "Break", &expr, target_keyword);
	}

	void Continue::dprint(std::ostream& out) const {
		internal::simpleActionDprint(out, "Continue", &expr, target_keyword);
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

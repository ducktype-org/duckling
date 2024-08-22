#include "preamble.hpp"
#include "../../hierarchy/actions.hpp"

namespace pst {
	ParserRef<Action> Action::parse(RiftParserState& state) {
		dia::SourcePosition position = state.getPosition();

		if (!assertStmtChoice<Action>(state, state[0].isKeyword())) return nullptr;

		ParserRef<Action> out;
		auto              keyword = state[0].asKeyword();

		switch (keyword) {
		case Keyword::Return:
			out = makeRef<Return>(position);
			break;
		case Keyword::Break:
			out = makeRef<Break>(position);
			break;
		case Keyword::Continue:
			out = makeRef<Continue>(position);
			break;
		case Keyword::Redo:
			out = makeRef<Redo>(position);
			break;
		case Keyword::Restart:
			out = makeRef<Restart>(position);
			break;
		case Keyword::Defer:
			out = makeRef<Defer>(position);
			break;
		case Keyword::Throw:
			out = makeRef<Throw>(position);
			break;
		default:
			assertStmtChoice<Action>(state, false);
			return nullptr;
		}
		state.parse(out).eatOne();

		// @TODO: for now we assume if there is no expression there is a semicolon
		if (!state[0].is(Special::Semicolon))
			state.parse(out).with<Expr>(&out->expr, Expr::parse, true);

		return out;
	}

	namespace {
		void simpleActionDprint(
			std::ostream&                          out,
			const base::Optional<ParserRef<Expr>>& action,
			const std::string_view                 name,
			const std::string_view                 preposition
		) {
			out << R"({"Action": {)";
			out << R"("kind": ")" << name << "\"";
			if (action) {
				out << ", \"" << preposition << "\": ";
				nullAwareDprint(action.value(), out);
			}
			out << "}}";
		}

		void simpleActionSemPrint(
			std::ostream&                          out,
			const base::Optional<ParserRef<Expr>>& action,
			const std::string_view                 name,
			const std::string_view                 preposition
		) {
			out << R"({"Action": {)";
			position.semPrint(out);
			out << R"(,"semanticTokenType": "event",)";
			out << R"("kind": ")" << name << "\"";
			if (action) {
				out << ", \"" << preposition << "\": ";
				nullAwareSemanticTokenPrint(action.value(), out);
			}
			out << "}}";
		}
	}

	void Return::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Return", "with");
	}

	void Break::dprint(std::ostream& out) const { simpleActionDprint(out, expr, "Break", "from"); }

	void Continue::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Continue", "with");
	}

	void Redo::dprint(std::ostream& out) const { simpleActionDprint(out, expr, "Redo", "what"); }

	void Restart::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Restart", "what");
	}

	void Defer::dprint(std::ostream& out) const { simpleActionDprint(out, expr, "Defer", "what"); }

	void Throw::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Throw", "exception");
	}

	// Semantic printing
	void Return::semPrint(std::ostream& out) const {
		simpleActionSemPrint(out, expr, "Return", "with");
	}

	void Break::semPrint(std::ostream& out) const { simpleActionSemPrint(out, expr, "Break", "from"); }

	void Continue::semPrint(std::ostream& out) const {
		simpleActionSemPrint(out, expr, "Continue", "with");
	}

	void Redo::semPrint(std::ostream& out) const { simpleActionSemPrint(out, expr, "Redo", "what"); }

	void Restart::semPrint(std::ostream& out) const {
		simpleActionSemPrint(out, expr, "Restart", "what");
	}

	void Defer::semPrint(std::ostream& out) const { simpleActionSemPrint(out, expr, "Defer", "what"); }

	void Throw::semPrint(std::ostream& out) const {
		simpleActionSemPrint(out, expr, "Throw", "exception");
	}

	void Return::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitReturn(*this); }

	void Break::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitBreak(*this); }

	void Continue::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitContinue(*this); }

	void Redo::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitRedo(*this); }

	void Restart::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitRestart(*this); }

	void Defer::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitDefer(*this); }

	void Throw::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitThrow(*this); }


}

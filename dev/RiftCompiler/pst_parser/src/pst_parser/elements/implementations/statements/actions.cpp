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

	void Action::dprintPrefix(std::ostream& out) const {
		Stmt::dprintPrefix(out);
		out << R"({"kind": )";
	}

	void Action::dprintSuffix(std::ostream& out) const {
		if (expr) {
			out << R"(, "value":)";
			nullAwareDprint(expr.value(), out);
		}
		out << "}";
		Stmt::dprintSuffix(out);
	}

	void Return::dprint(std::ostream& out) const {
		out << "\"Return\"";
	}

	void Break::dprint(std::ostream& out) const {
		out << "\"Break\"";
	}

	void Continue::dprint(std::ostream& out) const {
		out << "\"Continue\"";
	}

	void Redo::dprint(std::ostream& out) const {
		out << "\"Redo\"";
	}

	void Restart::dprint(std::ostream& out) const {
		out << "\"Restart\"";
	}

	void Defer::dprint(std::ostream& out) const { 
		out << "\"Defer\"";
	}

	void Throw::dprint(std::ostream& out) const {
		out << "\"Throw\"";
	}

	void Return::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitReturn(*this); }

	void Break::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitBreak(*this); }

	void Continue::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitContinue(*this); }

	void Redo::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitRedo(*this); }

	void Restart::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitRestart(*this); }

	void Defer::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitDefer(*this); }

	void Throw::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitThrow(*this); }


}

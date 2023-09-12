#include "elements_implementation.hpp"

namespace pst {
	ParserRef<Action> Action::parse(RiftParserState& state) {

		RIFT_ASSERT(state.ctokens().isKeyword(), "bad statement choice");

		ParserRef<Action> out;
		auto keyword = state.ctokens().peek().asKeyword();
		lexer::SourcePosition position = state.ctokens().peek().getPosition();

		switch (keyword) {
		case Keyword::Return:
			out = makeRef<Return>();
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
			RIFT_PANIC("bad statement choice");
		}
		state.tokens().skip();

		// @TODO: for now we assume if there is no expression there is a semicolon
		if (!state.ctokens().is(Special::Semicolon))
			out->expr = Expr::parse(state);

		return out;
	}	

	namespace {
		void simpleActionDprint(std::ostream& out, const std::optional<ParserRef<Expr>> &action, const std::string_view name, std::string preposition) {
			out << "{\"" << name << "\"";
			if (action)  {
				out << " : {\"" << preposition << "\": ";
				nullAwareDprint(action.value(), out);
				out << "}";
			}
			out << "}";
		}
	}

	void Return::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Return", "with");
	}

	void Break::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Break", "from");
	}

	void Continue::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Continue", "with");
	}

	void Redo::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Redo", "what");
	}

	void Restart::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Restart", "what");
	}

	void Defer::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Defer", "what");
	}

	void Throw::dprint(std::ostream& out) const {
		simpleActionDprint(out, expr, "Throw", "exception");
	}
}

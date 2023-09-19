#include "elements_implementation.hpp"

namespace pst {

	namespace detail {

		template<class T>
		ParserRef<T> parseStmt(RiftParserState& state, bool force_semi = false) {
			ParserRef<T> out = T::parse(state);
			if (force_semi or out->trailingSemicolon()) parseOne(state, Special::Semicolon);
			return out;
		}
	}

	bool Stmt::trailingSemicolon() {
		return true;
	}

	ParserRef<Stmt> Stmt::parse(RiftParserState& state) {
		auto as_keyword = state.ctokens().peek().asKeyword();
		auto as_special = state.ctokens().peek().asSpecial();

		switch (as_keyword) {
		case Keyword::If:
			return detail::parseStmt<If>(state);

		case Keyword::Fun:
			return detail::parseStmt<Fun>(state);

		case Keyword::While:
			return detail::parseStmt<While>(state);

		case Keyword::Import:
			return detail::parseStmt<Import>(state);

		case Keyword::Using:
			return detail::parseStmt<Using>(state);

		case Keyword::Namespace:
			return detail::parseStmt<Namespace>(state);

		case Keyword::Struct:
			return detail::parseStmt<Struct>(state);

		case Keyword::Block:
			return detail::parseStmt<Block>(state);

		case Keyword::Const:
			return detail::parseStmt<Const>(state);

		case Keyword::Alias:
			return detail::parseStmt<Alias>(state);

		case Keyword::RiftTestEagerLookup:
			return detail::parseStmt<EagerLookup>(state);

		default:
			break;
		}

		if (as_special == Special::AtSign) return detail::parseStmt<Attribute>(state);
		if (rift_def::keywordFlags(as_keyword).contains(rift_def::KeywordFlags::is_action))
			return detail::parseStmt<Action>(state);

		if (as_special == Special::Semicolon) {
			state.fail(0, "unexpected special `;`");
			state.tokens().skip();
			return nullptr;
		}

		// Expr as stmt have semicolon at the end:
		return detail::parseStmt<Expr>(state, true);
	}
}

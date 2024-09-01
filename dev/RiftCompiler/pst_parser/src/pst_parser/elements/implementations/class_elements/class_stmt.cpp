#include "preamble.hpp"

namespace pst {
	namespace detail {
		template<std::derived_from<ClassStmt> T, class... Ts>
		ParserRef<T> parseStmt(RiftParserState& state, Ts... args) {
			ParserRef<T> out = T::parse(state, std::forward<Ts...>(args)...);
			if (out->trailingSemicolon()) state.parse(out).one(Special::Semicolon);
			return out;
		}
	}

	i64 ClassStmt::countSpecifiers(RiftParserState& state) {
		i64 res = 0;
		while (class_specs.contains(state[res].asKeyword())) res++;
		return res;
	}

	void ClassStmt::parseSpecifiers(RiftParserState& state) {
		while (class_specs.contains(state[0].asKeyword())) {
			context.specifiers.emplace_back(&state[0]);
			state.parse(base::borrow_ptr(this)).eatOne();
		}
	}

	void ClassStmt::dprintPrefix(std::ostream& out) const {
		Stmt::dprintPrefix(out);
		if (context.specifiers.size()) {
			out << R"("specifiers":[)";
			for (auto& spec: context.specifiers) {
				tpc::nullAwareDprint(spec->asKeyword(), out);
				out << ",";
			}
			out << "],";
		}
	}

	ParserRef<ClassStmt> ClassStmt::chooseStmt(RiftParserState& state, const ClassContext& ctx) {
		i64 skip = countSpecifiers(state);

		Keyword as_keyword = state[skip].asKeyword();

		switch (as_keyword) {
		case Keyword::Fun:
			return detail::parseStmt<Method>(state, ctx);
		case Keyword::Const:
			return detail::parseStmt<Field>(state, ctx);
		default:
			break;
		}

		if (state[skip].isStr(ctx.name)) return detail::parseStmt<ClassSpecial>(state, ctx);
		if (state[skip].isBracketGroup(Token::Curly))
			return detail::parseStmt<AccessBlock>(state, ctx);

		return detail::parseStmt<Field>(state, ctx);
	}

	tpc::ParserRef<ClassStmt> ClassStmt::parse(RiftParserState& state, const ClassContext& ctx) {
		// Collect Attributes
		auto attributes = collectAttributes(state);

		// Parse Statement
		ParserRef<ClassStmt> out = chooseStmt(state, ctx);

		// Add Attributes
		if (out != nullptr) out->addAttributes(std::move(attributes));

		return out;
	}
}

#include "preamble.hpp"

namespace pst {
	namespace detail {
		template<std::derived_from<ClassStmt> T, class ...Ts>
		ParserRef<T> parseStmt(RiftParserState& state, Ts... args) {
			ParserRef<T> out = T::parse(state, args...);
			if (out->trailingSemicolon()) state.parse(out).one(Special::Semicolon);
			return out;
		}
	}
	i64 ClassStmt::countSpecifiers(RiftParserState& state) {
		i64 res = 0;
		while(class_specs.contains(state[res].asKeyword())) res++;
		return res;
	}

	void ClassStmt::parseSpecifiers(RiftParserState& state) {
		while(class_specs.contains(state[0].asKeyword())) {
			state.parse(base::borrow_ptr(this)).eatOne();
		}
	}

	void ClassStmt::dprintPrefix(std::ostream& out) const {
		Stmt::dprintPrefix(out);
		if (specifiers.size()) {
			out << R"("specifiers":[)";
			for(auto& spec: specifiers) {
				tpc::nullAwareDprint(spec, out);
				out << ",";
			}
			out << "],";
		}
	}

	ParserRef<ClassStmt> ClassStmt::chooseStmt(RiftParserState& state, tpc::Identifier class_name) {
		i64 skip = countSpecifiers(state);

		Keyword as_keyword = state[skip].asKeyword();

		switch(as_keyword) {
		case Keyword::Fun:
			return detail::parseStmt<Method>(state);
		case Keyword::Const:
			return detail::parseStmt<Field>(state);
		default:
			break;
		}

		if(state[skip].isStr(class_name)) {
			return detail::parseStmt<ClassSpecial>(state);
		}
		if(state[skip].isBracketGroup(Token::Curly)) {
			return detail::parseStmt<AccessBlock>(state, class_name);
		}
		
		return detail::parseStmt<Field>(state);
	}

	tpc::ParserRef<ClassStmt> ClassStmt::parse(RiftParserState& state, tpc::Identifier class_name) {
		// Collect Attributes
		auto attributes = collectAttributes(state);

		// Parse Statement
		ParserRef<ClassStmt> out = chooseStmt(state, class_name);
		
		// Add Attributes
		if (out != nullptr) {
			out->addAttributes(std::move(attributes));
		}

		return out;
	}
}
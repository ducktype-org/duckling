#include "../../hierarchy/class_elements/all_class_elements.hpp"  // IWYU pragma: keep
#include "../../hierarchy/lists/all_lists.hpp"                    // IWYU pragma: keep
#include "../../hierarchy/meta.hpp"
#include "../../hierarchy/not_statements/all_not_statements.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	namespace internal {
		template<std::derived_from<ClassStmt> T, class... Ts>
		MBox<T> parseStmt(LangParserState& state, Ts... args) {
			MBox<T> out = T::parse(state, std::forward<Ts...>(args)...);
			auto    opt = out.toOpt();
			if (opt && opt.value()->trailingSemicolon())
				state.parse(opt.value()).one(Special::Semicolon);
			return out;
		}
	}

	i64 ClassStmt::countSpecifiers(LangParserState& state) {
		i64 res = 0;
		while (class_specs.contains(state[res].asKeyword())) res++;
		return res;
	}

	void ClassStmt::parseSpecifiers(LangParserState& state) {
		while (class_specs.contains(state[0].asKeyword())) {
			context.specifiers.emplace_back(&state[0]);
			state.parse(Ref(this)).eatOne();
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

	MBox<ClassStmt> ClassStmt::chooseStmt(LangParserState& state, const ClassContext& ctx) {
		i64 skip = countSpecifiers(state);

		Keyword as_keyword = state[skip].asKeyword();

		switch (as_keyword) {
		case Keyword::Fun:
			return internal::parseStmt<Method>(state, ctx);
		case Keyword::Const:
			return internal::parseStmt<Field>(state, ctx);
		case Keyword::Pattern:
		case Keyword::Alias:
		case Keyword::Using:
			return internal::parseStmt<NonClassStmt>(state, ctx);
		default:
			break;
		}

		if (state[skip].isStr(ctx.name)) return internal::parseStmt<ClassSpecial>(state, ctx);
		if (state[skip].isBracketGroup(Token::Curly))
			return internal::parseStmt<AccessBlock>(state, ctx);

		return internal::parseStmt<Field>(state, ctx);
	}

	LangElement::HashAlg& ClassStmt::addGenericDataToHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, attributes.size());
		addToHash(partial_hash, context.name.str());
		addToHash(partial_hash, context.specifiers);
		return partial_hash;
	}

	MBox<ClassStmt> ClassStmt::parse(LangParserState& state, const ClassContext& ctx) {
		// Collect Attributes
		auto attributes = collectAttributes(state);

		// Parse Statement
		MBox<ClassStmt> out = chooseStmt(state, ctx);

		// Add Attributes
		if (out) out->addAttributes(state, std::move(attributes));

		return out;
	}
}

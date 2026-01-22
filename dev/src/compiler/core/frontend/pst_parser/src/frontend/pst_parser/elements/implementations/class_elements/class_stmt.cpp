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
			PST_RETURN out;
		}
	}

	MBox<ClassStmt> ClassStmt::chooseStmt(LangParserState& state, const ClassContext& ctx) {
		Keyword as_keyword = state[0].asKeyword();

		switch (as_keyword) {
		case Keyword::Fun:
			return internal::parseStmt<Method>(state, ctx);
		case Keyword::Let:
		case Keyword::Var:
			return internal::parseStmt<Field>(state, ctx);
		case Keyword::Alias:
		case Keyword::Using:
		case Keyword::Class:
			return internal::parseStmt<NonClassStmt>(state, ctx);
		default:
			break;
		}

		if (state[0].isStr(ctx.name)) return internal::parseStmt<ClassSpecial>(state, ctx);

		return internal::parseStmt<Field>(state, ctx);
	}

	LangElement::HashAlg& ClassStmt::addGenericDataToHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, prefixes.attributes.size());
		addToHash(partial_hash, prefixes.specifiers.size());
		addToHash(partial_hash, context.name.str());
		return partial_hash;
	}

	MBox<ClassStmt> ClassStmt::parse(LangParserState& state, const ClassContext& ctx) {
		// Collect Attributes
		auto prefixes = collectPrefixes(state);

		MBox<ClassStmt> out;

		// Specifier block handling
		if (!prefixes.specifiers.empty() && state[0].isBracketGroup(Token::Curly)) {
			out = internal::parseStmt<ClassSpecifierBlock>(state, ctx);
		} else {
			// Parse Statement
			out = chooseStmt(state, ctx);
		}

		// Add Attributes
		if (out) out->addPrefixes(state, std::move(prefixes));

		PST_RETURN out;
	}
}

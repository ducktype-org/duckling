// #include "../../hierarchy/class_elements/all_class_elements.hpp"  // IWYU pragma: keep
// #include "../../hierarchy/lists/all_lists.hpp"                    // IWYU pragma: keep
// #include "../../hierarchy/meta.hpp"
// #include "../../hierarchy/not_statements/all_not_statements.hpp"  // IWYU pragma: keep
// #include "../../implementations/meta/meta_errors.hpp"
// #include "../../lang_state_unmethods.hpp"
// #include "preamble.hpp"

// namespace pst {
	// namespace {
		// using namespace internal;

		// template<class T>
		// struct StmtClassifiers {
			// /**
			 // * @brief Function that checks heuristically for a potential end of a typical class
			 // * statement.
			 // *
			 // * Sentinel just indicates there are no more tokens.
			 // * The typical valid ends are:
			 // *  - `;` being the end of a statement.
			 // *  - `{}` being the end of a statement.
			 // * The heuristics that check that a new statement seems to start are:
			 // *  - `@` being the start of an attribute which can only be at the begining of a
			 // * statement.
			 // *  - A keyword that is always at the start of a statement.
			 // *  - A keyword that is a specifier.
			 // *
			 // * @note This doesn't handle current class specials with semicolons missing beforehand.
			 // */
			// static bool isStmtEnd(const TokenStream& state, i64 fwd) {
				// return state[fwd].is(Token::Type::Sentinel) || state[fwd].is(Special::AtSign)
				    // || state[fwd - 1].is(Special::Semicolon)
				    // || keywordFlags(state[fwd].asKeyword())
				           // .contains(lang_def::KeywordFlagsOptions::IsStmtStart)
				    // || keywordFlags(state[fwd].asKeyword())
				           // .contains(lang_def::KeywordFlagsOptions::IsSpecifier)
				    // || Conditions::isBlockGroup(state, fwd - 1);
			// }
		// };

		// template<std::derived_from<ClassStmt> T, class... Ts>
		// MBox<T> parseStmt(LangParserState& state, Ts... args) {
			// // We skip the first token as its the keyword we already found
			// u64  length = 1 + state.ctokens().countUntil<StmtClassifiers<T>::isStmtEnd>(1);
			// bool could_implicitly_return
				// = !state[base::safeIntConv<i64>(length) - 1].is(Special::Semicolon)
			   // && state[base::safeIntConv<i64>(length)].is(Token::Type::Sentinel);

			// fallbackLen(state, length);

			// MBox<T> out = T::parse(state, std::forward<Ts...>(args)...);

			// auto opt = out.toOpt();
			// if (opt && opt.value()->trailingSemicolon()) {
				// if (could_implicitly_return)
					// makeImplicitReturn(out.refMut());
				// else
					// state.parse(opt.value()).one(Special::Semicolon);
			// }

			// exitFallback(state);

			// PST_RETURN out;
		// }

		// MBox<ClassStmt> chooseStmt(LangParserState& state) {
			// if (state[0].is(Special::Semicolon)
			    // && (state[-1].is(Special::Semicolon) || isSentinel(state, -1))) {
				// state.tokens().skip();
				// return nullptr;
			// }

			// if (state[0].is(Special::Semicolon) || isSentinel(state, 0)) {
				// state.logInt(base::makeBox<EmptyStatementError>(state.getPosition()));
				// return nullptr;
			// }

			// Keyword as_keyword = state[0].asKeyword();

			// switch (as_keyword) {
			// case Keyword::Fun:
				// return parseStmt<Method>(state);
			// case Keyword::Let:
			// case Keyword::Var:
				// return parseStmt<Field>(state);
			// case Keyword::Alias:
			// case Keyword::Using:
			// case Keyword::Class:
				// return parseStmt<NonClassStmt>(state);
			// default:
				// break;
			// }

			// if (state[0].isStr(state.getContext()->class_name))
				// return parseStmt<ClassSpecial>(state);

			// return parseStmt<Field>(state);
		// }
	// }

	// HashAlg& ClassStmt::addGenericDataToHash(HashAlg& partial_hash) const {
		// addToHash(partial_hash, prefixes.attributes.size());
		// addToHash(partial_hash, prefixes.specifiers.size());
		// addToHash(partial_hash, isImplicitReturn());
		// return partial_hash;
	// }

	// MBox<ClassStmt> ClassStmt::parse(LangParserState& state) {
		// // Collect Attributes
		// auto prefixes = collectPrefixes(state);

		// MBox<ClassStmt> out;

		// // Specifier block handling
		// if (!prefixes.specifiers.empty() && state[0].isBracketGroup(Token::Curly)) {
			// out = parseStmt<ClassSpecifierBlock>(state);
		// } else {
			// // Parse Statement
			// out = chooseStmt(state);
		// }

		// // Add Attributes
		// if (out) out->addPrefixes(state, std::move(prefixes));

		// PST_RETURN out;
	// }
// }

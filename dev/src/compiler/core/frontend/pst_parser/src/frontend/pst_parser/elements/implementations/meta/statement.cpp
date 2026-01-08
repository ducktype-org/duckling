#include "../../hierarchy/declarations/all_declarations.hpp"
#include "../../hierarchy/lists/all_lists.hpp"                    // IWYU pragma: keep
#include "../../hierarchy/not_statements/all_not_statements.hpp"  // IWYU pragma: keep
#include "../../hierarchy/statements/all_statements.hpp"
#include "preamble.hpp"

#include "meta_errors.hpp"

namespace pst {

	bool Stmt::trailingSemicolon() { return true; }
	namespace internal {

		template<class T>
		struct StmtClassifiers {
			/**
			 * @brief Function that checks heuristically for a potential end of a typical statement.
			 *
			 * Sentinel just indicates there are no more tokens.
			 * The typical valid ends are:
			 *  - `;` being the end of a statement.
			 *  - `{}` being the end of a statement.
			 * The heuristics that check that a new statement seems to start are:
			 *  - `@` being the start of an attribute which can only be at the begining of a
			 * statement.
			 *  - A keyword that is always at the start of a statement.
			 *  - A keyword that is a specifier.
			 */
			static bool isStmtEnd(const TokenStream& state, i64 fwd) {
				return state[fwd].is(Token::Type::Sentinel) || state[fwd].is(Special::AtSign)
				    || state[fwd - 1].is(Special::Semicolon)
				    || keywordFlags(state[fwd].asKeyword())
				           .contains(lang_def::KeywordFlagsOptions::IsStmtStart)
				    || keywordFlags(state[fwd].asKeyword())
				           .contains(lang_def::KeywordFlagsOptions::IsSpecifier)
				    || Conditions::isBlockGroup(state, fwd - 1);
			}
		};

		template<>
		struct StmtClassifiers<ExprStmt> {
			/**
			 * @brief Function that checks heuristically for a potential end of an expression
			 * statement.
			 *
			 * The difference from the general function is that `{}` doesn't indicate the end of an
			 * expression statement.
			 */
			static bool isStmtEnd(const TokenStream& state, i64 fwd) {
				return state[fwd].is(Token::Type::Sentinel) || state[fwd].is(Special::AtSign)
				    || state[fwd - 1].is(Special::Semicolon)
				    || keywordFlags(state[fwd].asKeyword())
				           .contains(lang_def::KeywordFlagsOptions::IsStmtStart)
				    || keywordFlags(state[fwd].asKeyword())
				           .contains(lang_def::KeywordFlagsOptions::IsSpecifier);
			}
		};

		template<>
		struct StmtClassifiers<If> {
			/**
			 * @brief Function that checks heuristically for a potential end of an if statement.
			 *
			 * This function is a very rough placeholder that should work in most correct cases but
			 * a proper heuristic handling will be needed.
			 *
			 * @TODO: #1761 Add proper handling instead.
			 */
			static bool isStmtEnd(const TokenStream& state, i64 fwd) {
				return state[fwd].is(Token::Type::Sentinel)
				    || ((state[fwd - 1].is(Special::Semicolon)
				         || Conditions::isBlockGroup(state, fwd - 1))
				        && !state[fwd].is(Keyword::Else));
			}
		};

		template<>
		struct StmtClassifiers<StmtSpecifier> {
			/**
			 * @brief Function that checks heuristically for a potential end of a
			 * specifier statement.
			 *
			 * This function is a very rough placeholder that will be replaced
			 * with the rework of how specifiers work
			 *
			 * @TODO: #1746 Will remove this part.
			 */
			static bool isStmtEnd(const TokenStream& state, i64 fwd) {
				return state[fwd].is(Token::Type::Sentinel) || state[fwd].is(Special::AtSign)
				    || state[fwd - 1].is(Special::Semicolon)
				    || (Conditions::isBlockGroup(state, fwd - 1) && !state[fwd].is(Keyword::Else));
			}
		};

		template<std::derived_from<Stmt> T>
		MBox<T> parseStmt(LangParserState& state) {
			// We skip the first token as its the keyword we already found
			u64 length = 1 + state.ctokens().countUntil<StmtClassifiers<T>::isStmtEnd>(1);

			fallbackLen(state, length);

			MBox<T> out = T::parse(state);

			auto opt = out.toOpt();
			if (opt && opt.value()->trailingSemicolon())
				state.parse(opt.value()).one(Special::Semicolon);

			exitFallback(state);

			return out;
		}

		template<>
		MBox<StmtSpecifier> parseStmt(LangParserState& state) {
			// We skip the first token as its the keyword we already found
			i64 length = 1;

			PST_WHILE(!StmtClassifiers<StmtSpecifier>::isStmtEnd(state.ctokens(), length)) length++;

			fallbackLen(state, base::safeIntConv<u64>(length));

			MBox<StmtSpecifier> out = StmtSpecifier::parse(state);

			exitFallback(state);

			return out;
		}

		MBox<Stmt> chooseStmt(LangParserState& state) {
			if (state[0].is(Special::Semicolon)
			    && (state[-1].is(Special::Semicolon) || isSentinel(state, -1))) {
				state.tokens().skip();
				return nullptr;
			}

			if (state[0].is(Special::Semicolon) || isSentinel(state, 0)) {
				state.logInt(base::makeBox<EmptyStatementError>(state.getPosition()));
				return nullptr;
			}

			Keyword as_keyword = state[0].asKeyword();

			switch (as_keyword) {
			case Keyword::If:
				return internal::parseStmt<If>(state);

			case Keyword::Fun:
				return internal::parseStmt<Fun>(state);

			case Keyword::FunDecl:
				return internal::parseStmt<FunDecl>(state);

			case Keyword::Pattern:
				return internal::parseStmt<Pattern>(state);

			case Keyword::While:
				return internal::parseStmt<While>(state);

			case Keyword::For:
				return internal::parseStmt<For>(state);

			case Keyword::Import:
				return internal::parseStmt<Import>(state);

			case Keyword::Using:
				return internal::parseStmt<Using>(state);

			case Keyword::Namespace:
				return internal::parseStmt<Namespace>(state);

			case Keyword::Class:
				return internal::parseStmt<Class>(state);

			case Keyword::Block:
				return internal::parseStmt<Block>(state);

			case Keyword::Const:
				return internal::parseStmt<Const>(state);

			case Keyword::Alias:
				return internal::parseStmt<Alias>(state);

			case Keyword::Var:
			case Keyword::Let:
				return internal::parseStmt<Variable>(state);

			case Keyword::Expand:
				return internal::parseStmt<Expand>(state);

			default:
				if (StmtSpecifier::SPECIFIERS.contains(as_keyword))
					return internal::parseStmt<StmtSpecifier>(state);
				break;
			}

			if (lang_def::keywordFlags(as_keyword).contains(lang_def::KeywordFlagsOptions::IsAction))
				return internal::parseStmt<Action>(state);

			// Expr as stmt have semicolon at the end:
			return internal::parseStmt<ExprStmt>(state);
		}
	}

	Stmt::AttrBoxList Stmt::collectAttributes(LangParserState& state) {
		auto        as_special = state[0].asSpecial();
		AttrBoxList attributes;

		PST_WHILE(as_special == Special::AtSign) {
			MBox<Attribute> attr = Attribute::parse(state);
			auto            opt  = std::move(attr).toOptBox();
			if (opt) attributes.emplace_back(std::move(opt.value()));
			as_special = state[0].asSpecial();
		}
		return attributes;
	}

	MBox<Stmt> Stmt::parse(LangParserState& state) {
		// Collect Attributes
		auto attributes = collectAttributes(state);

		// Parse Statement
		MBox<Stmt> out = internal::chooseStmt(state);

		// Add Attributes
		if (out) out->addAttributes(state, std::move(attributes));

		return out;
	}

	LangElement::HashAlg& Stmt::addGenericDataToHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, attributes.size());
		// note: Value of Kind should be strictly implied by elementType, that is added to hash for
		// each element
		return partial_hash;
	}

	void Stmt::calcElementPathHashRecursive() {
		auto                   path       = getElementPathHash();
		hashing::ComponentHash attrs_path = { path, "attributes" };
		calcIndexedListChildPath<Attribute>({ attributes }, attrs_path);
		for (auto& el: sub_elements) {
			variant_match(el) {
				variant_case(InternalChild, child) {
					if (child->getElementKind() == ElementKind::Attribute) continue;
					CORE_PANIC(
						"Default implementation of calculating element paths cannot handle unnamed "
						"sub-elements. Encountered while calculating for: "
						+ elementType()
					);
				}
				variant_case(InternalNamedChild, named_child) {
					hashing::ComponentHash child_path(path, named_child.name);
					named_child.element->calcElementPathHash(child_path);
				}
			}
		}
	}

	void Stmt::dprintPrefix(std::ostream& out) const {
		LangElement::dprintPrefix(out);
		dprintAttributes(out);
	}

	void Stmt::dprintAttributes(std::ostream& out) const {
		if (not attributes.empty()) {
			out << R"("attributes": [)";
			for (auto& attribute: attributes) {
				attribute.internal()->debugPrint(out);
				out << ",";
			}
			out << "],";
		}
	}

	void Stmt::addAttributes(LangParserState& state, AttrBoxList&& additions) {
		attributes.resize(additions.size());
		usize i = 0;
		for (auto&& attr_add: std::move(additions)) {
			state.parse(Ref(this)).assign(&attributes[i], MBox(std::move(attr_add)));
			i++;
		}

		if (attributes.size() > 0)
			setFirstToken(attributes.front().internal()->getSourcePosition());
	}
}

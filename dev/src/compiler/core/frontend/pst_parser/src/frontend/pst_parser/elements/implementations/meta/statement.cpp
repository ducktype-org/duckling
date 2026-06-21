#include "../../hierarchy/declarations/all_declarations.hpp"
#include "../../hierarchy/lists/all_lists.hpp"                    // IWYU pragma: keep
#include "../../hierarchy/not_statements/all_not_statements.hpp"  // IWYU pragma: keep
#include "../../hierarchy/statements/all_statements.hpp"
#include "meta_errors.hpp"
#include "preamble.hpp"

namespace pst {

	CLONE_SUB_ELEMENTS_DEF(Stmt, prefixes.attributes, prefixes.specifiers);

	bool Stmt::trailingSemicolon() { return true; }

	namespace internal {
		void makeImplicitReturn(MRef<Stmt> box) { box->makeImplicitReturn(); }

		bool isStatementBegin(const tpc::TokenStream& state, i64 fwd) {
			return state[fwd].is(Token::Type::Sentinel) || state[fwd].is(Special::AtSign)
			    || state[fwd - 1].is(Special::Semicolon)
			    || keywordFlags(state[fwd].asKeyword())
			           .contains(lang_def::KeywordFlagsOptions::IsStmtStart)
			    || keywordFlags(state[fwd].asKeyword())
			           .contains(lang_def::KeywordFlagsOptions::IsSpecifier);
		}

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
				if (state[fwd].is(Token::Type::Sentinel)) return true;

				if (state[fwd].is(Special::AtSign)) return true;

				if (state[fwd - 1].is(Special::Semicolon)) return true;

				if (keywordFlags(state[fwd].asKeyword())
				        .contains(lang_def::KeywordFlagsOptions::IsStmtStart))
					return true;

				if (keywordFlags(state[fwd].asKeyword())
				        .contains(lang_def::KeywordFlagsOptions::IsSpecifier))
					return true;

				// When we have two curly bracket blocks as consecutive tokens, it probably means
				// that the user intended to write two statements
				if (state[fwd - 1].isBracketGroup(lexer::Token::BracketType::Curly)
				    && state[fwd].isBracketGroup(lexer::Token::BracketType::Curly))
					return true;

				return false;
			}
		};

		template<class T>
		struct StmtFinder {
			/**
			 * @brief Calculates the heuristic for where a given statement ends. Can be overriden
			 * when needed.
			 */
			static u64 findStatementLength(LangParserState& state) {
				return 1 + state.ctokens().countUntil<StmtClassifiers<T>::isStmtEnd>(1);
			}
		};

		template<class T>
		concept FunctionLike = std::same_as<T, Fun> || std::same_as<T, Pattern>;

		template<class T>
		requires FunctionLike<T> struct StmtFinder<T> {
		private:
			static bool isAssignOrEnd(const TokenStream& stream, i64 fwd) {
				return StmtClassifiers<T>::isStmtEnd(stream, fwd)
				    || stream[fwd].is(NamedOperator::Assign);
			}

			static bool isInnerExprEnd(const TokenStream& stream, i64 fwd) {
				return StmtClassifiers<ExprStmt>::isStmtEnd(stream, fwd);
			}

		public:
			/**
			 * @brief For function like definitions we have to consider code block vs an expression.
			 */
			static u64 findStatementLength(LangParserState& state) {
				u64 initial_length = 1 + state.ctokens().countUntil<isAssignOrEnd>(1);
				if (!state[base::safeIntConv<i64>(initial_length)].is(NamedOperator::Assign))
					return initial_length;
				initial_length++;
				if (Conditions::isBlockGroup(
						state.ctokens(), base::safeIntConv<i64>(initial_length)
					))
					return initial_length + 1;
				return initial_length
				     + state.ctokens().countUntil<isInnerExprEnd>(
						 base::safeIntConv<i64>(initial_length)
					 );
			}
		};

		template<std::derived_from<Stmt> T>
		MBox<T> parseStmt(LangParserState& state) {
			// We skip the first token as its the keyword we already found
			u64  length = StmtFinder<T>::findStatementLength(state);
			bool could_implicitly_return
				= !state[base::safeIntConv<i64>(length) - 1].is(Special::Semicolon)
			   && state[base::safeIntConv<i64>(length)].is(Token::Type::Sentinel);

			fallbackLen(state, length);

			MBox<T> out = T::parse(state);

			auto opt = out.toOpt();
			if (opt && opt.value()->trailingSemicolon()) {
				if (could_implicitly_return)
					makeImplicitReturn(out.refMut());
				else
					state.parse(opt.value()).one(Special::Semicolon);
			}

			exitFallback(state);

			PST_RETURN out;
		}

		/**
		 * @brief This is a helper concept for Statements that behave similarly to flow control
		 * elements as in they can have a code block or a sub-statement. They have to be handled
		 * differently for fallbacks.
		 */
		template<class T>
		concept FlowControlLike
			= std::same_as<T, If> || std::same_as<T, For> || std::same_as<T, While>;

		template<std::derived_from<Stmt> T>
		requires FlowControlLike<T> MBox<T> parseStmt(LangParserState& state) {
			setSoftFallback(state, isStatementBegin);

			MBox<T> out = T::parse(state);

			exitSoftFallback(state);

			PST_RETURN out;
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
				break;
			}

			if (lang_def::keywordFlags(as_keyword).contains(lang_def::KeywordFlagsOptions::IsAction))
				return internal::parseStmt<Action>(state);

			// Expr as stmt have semicolon at the end:
			return internal::parseStmt<ExprStmt>(state);
		}
	}

	Stmt::PrefixBoxes Stmt::collectPrefixes(LangParserState& state) {
		auto        as_special = state[0].asSpecial();
		auto        as_keyword = state[0].asKeyword();
		PrefixBoxes collect;

		PST_WHILE(
			as_special == Special::AtSign
			|| lang_def::keywordFlags(as_keyword).contains(lang_def::KeywordFlagsOptions::IsSpecifier)
		) {
			if (as_special == Special::AtSign) {
				MBox<Attribute> attr = Attribute::parse(state);
				auto            opt  = std::move(attr).toOptBox();
				if (opt) collect.attributes.emplace_back(std::move(opt.value()));
			} else if (lang_def::keywordFlags(as_keyword)
			               .contains(lang_def::KeywordFlagsOptions::IsSpecifier)) {
				MBox<StmtSpecifier> spec = StmtSpecifier::parse(state);
				auto                opt  = std::move(spec).toOptBox();
				if (opt) collect.specifiers.emplace_back(std::move(opt.value()));
			}

			as_special = state[0].asSpecial();
			as_keyword = state[0].asKeyword();
		}
		return collect;
	}

	MBox<Stmt> Stmt::parse(LangParserState& state) {
		// Collect Attributes
		auto prefixes = collectPrefixes(state);

		MBox<Stmt> out;

		// Specifier block handling
		if (!prefixes.specifiers.empty() && state[0].isBracketGroup(Token::Curly)) {
			out = internal::parseStmt<SpecifierBlock>(state);
		} else {
			// Parse Statement
			out = internal::chooseStmt(state);
		}

		// Add Attributes
		if (out) out->addPrefixes(state, std::move(prefixes));

		PST_RETURN out;
	}

	HashAlg& Stmt::addGenericDataToHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, prefixes.attributes.size());
		addToHash(partial_hash, prefixes.specifiers.size());
		addToHash(partial_hash, isImplicitReturn());
		// note: Value of Kind should be strictly implied by elementType, that is added to hash for
		// each element
		return partial_hash;
	}

	void Stmt::calcElementPathHashRecursive() {
		auto                   path       = getElementPathHash();
		hashing::ComponentHash attrs_path = { path, "attributes" };
		calcIndexedListChildPath<Attribute>({ prefixes.attributes }, attrs_path);
		hashing::ComponentHash spec_path = { path, "specifiers" };
		calcIndexedListChildPath<StmtSpecifier>({ prefixes.specifiers }, spec_path);
		for (auto& el: sub_elements) {
			variant_match(el) {
				variant_case(InternalChild, child) {
					if (child->getElementKind() == ElementKind::Attribute
					    || child->getElementKind() == ElementKind::StmtSpecifier)
						continue;
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
		dprintPrefixes(out);
	}

	void Stmt::dprintPrefixes(std::ostream& out) const {
		if (not prefixes.attributes.empty()) {
			out << R"("attributes": [)";
			for (auto& attribute: prefixes.attributes) {
				attribute.internal()->debugPrint(out);
				out << ",";
			}
			out << "],";
		}
		if (not prefixes.specifiers.empty()) {
			out << R"("specifiers": [)";
			for (auto& attribute: prefixes.specifiers) {
				attribute.internal()->debugPrint(out);
				out << ",";
			}
			out << "],";
		}
		if (isImplicitReturn()) out << R"("implicit_return": 1,)";
	}

	void Stmt::addPrefixes(LangParserState& state, PrefixBoxes&& additions) {
		auto [attributes, specifiers] = std::move(additions);
		// Move attributes
		prefixes.attributes.resize(attributes.size());
		usize i = 0;
		for (auto&& attr_add: std::move(attributes)) {
			state.parse(Ref(this)).assign(&prefixes.attributes[i], MBox(std::move(attr_add)));
			i++;
		}

		if (prefixes.attributes.size() > 0)
			setFirstToken(prefixes.attributes.front().internal()->source_position);

		// Move specifiers
		prefixes.specifiers.resize(specifiers.size());
		i = 0;
		for (auto&& spec_add: std::move(specifiers)) {
			state.parse(Ref(this)).assign(&prefixes.specifiers[i], MBox(std::move(spec_add)));
			i++;
		}

		if (prefixes.specifiers.size() > 0)
			setFirstToken(prefixes.specifiers.front().internal()->source_position);
	}

	base::Optional<base::StrID> Stmt::getInternalSymbolName() const {
		return getDeclSymbolIdentifier().map([](const AccessLocked<IdentifierWrapper>& acc) {
			return acc.illegalAccess().value()->unwrap();
		});
	}
}

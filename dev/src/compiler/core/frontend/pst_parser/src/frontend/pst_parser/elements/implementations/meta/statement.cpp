// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/class_elements/all_class_elements.hpp"
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
		static bool isStatementStartOrSpecifier(const TokenStream& state, i64 fwd) {
			if (!state[fwd].isKeyword()) return false;
			auto flags = keywordFlags(state[fwd].asKeyword());
			return flags.contains(lang_def::KeywordFlagsOptions::IsStmtStart)
			    || flags.contains(lang_def::KeywordFlagsOptions::IsSpecifier);
		}

		void makeImplicitReturn(MRef<Stmt> box) { box->makeImplicitReturn(); }

		bool isStatementBegin(const tpc::TokenStream& state, i64 fwd) {
			return state[fwd].is(Token::Type::Sentinel) || state[fwd].is(Special::AtSign)
			    || state[fwd - 1].is(Special::Semicolon) || isStatementStartOrSpecifier(state, fwd);
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
				    || isStatementStartOrSpecifier(state, fwd)
				    || (Conditions::isBlockGroup(state, fwd - 1)
				        && !Conditions::isMatchBodyBlock(state, fwd - 1));
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

				if (isStatementStartOrSpecifier(state, fwd)) return true;

				// When we have two curly bracket blocks as consecutive tokens, it probably means
				// that the user intended to write two statements
				if (state[fwd - 1].isBracketGroup(lexer::Token::BracketType::Curly)
				    && state[fwd].isBracketGroup(lexer::Token::BracketType::Curly))
					return true;

				return false;
			}
		};

		/**
		 * @brief Statements whose body is a `SelectorList`.
		 */
		template<class T>
		concept SelectorStmt = std::same_as<T, Using> || std::same_as<T, Import>;

		template<class T>
		requires SelectorStmt<T> struct StmtClassifiers<T> {
			/**
			 * @brief Like the general function, but a `{}` does not end the statement: in
			 * `using a.{b, c};` and `import a.* hides {b, c};` the bracket is part of a selector.
			 */
			static bool isStmtEnd(const TokenStream& state, i64 fwd) {
				return state[fwd].is(Token::Type::Sentinel) || state[fwd].is(Special::AtSign)
				    || state[fwd - 1].is(Special::Semicolon)
				    || isStatementStartOrSpecifier(state, fwd);
			}
		};

		template<class T>
		struct StmtFinder {
			/**
			 * @brief Calculates the heuristic for where a given statement starting `base` tokens
			 * ahead ends. Can be overriden when needed.
			 */
			static u64 findStatementLength(LangParserState& state, i64 base = 0) {
				return 1 + state.ctokens().countUntil<StmtClassifiers<T>::isStmtEnd>(base + 1);
			}
		};

		template<>
		struct StmtFinder<Action> {
			static u64 findStatementLength(LangParserState& state) {
				if ((state[0].is(Keyword::Break) || state[0].is(Keyword::Continue))
				    && state.ctokens().size() >= 3 && state[1].isKeyword()
				    && state[2].is(Special::Semicolon)
				    && isControlFlowTargetKeyword(state[1].asKeyword()))
					return 3;
				return 1 + state.ctokens().countUntil<StmtClassifiers<Action>::isStmtEnd>(1);
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
			static u64 findStatementLength(LangParserState& state, i64 base = 0) {
				u64 initial_length = 1 + state.ctokens().countUntil<isAssignOrEnd>(base + 1);
				if (!state[base + base::safeIntConv<i64>(initial_length)].is(NamedOperator::Assign))
					return initial_length;
				initial_length++;
				i64 body = base + base::safeIntConv<i64>(initial_length);
				if (Conditions::isBlockGroup(state.ctokens(), body)) return initial_length + 1;
				return initial_length + state.ctokens().countUntil<isInnerExprEnd>(body);
			}
		};

		/**
		 * @brief Calls `visitor.template operator()<T>()` with the statement type chosen for the
		 * token `at` tokens ahead in the normal statement context. `has_specifiers` tells whether
		 * the statement was preceded by specifiers.
		 */
		template<class Visitor>
		decltype(auto
		) visitStmtKind(LangParserState& state, i64 at, bool has_specifiers, Visitor& visitor) {
			if (has_specifiers && state[at].isBracketGroup(Token::BracketType::Curly))
				return visitor.template operator()<SpecifierBlock>();

			Keyword as_keyword = state[at].asKeyword();

			switch (as_keyword) {
			case Keyword::Template:
				return visitor.template operator()<TemplateStmt>();
			case Keyword::If:
				return visitor.template operator()<If>();
			case Keyword::Fun:
				return visitor.template operator()<Fun>();
			case Keyword::FunDecl:
				return visitor.template operator()<FunDecl>();
			case Keyword::Pattern:
				return visitor.template operator()<Pattern>();
			case Keyword::While:
				return visitor.template operator()<While>();
			case Keyword::For:
				return visitor.template operator()<For>();
			case Keyword::Import:
				return visitor.template operator()<Import>();
			case Keyword::Using:
				return visitor.template operator()<Using>();
			case Keyword::Namespace:
				return visitor.template operator()<Namespace>();
			case Keyword::Class:
				return visitor.template operator()<Class>();
			case Keyword::Block:
				return visitor.template operator()<Block>();
			case Keyword::Const:
				return visitor.template operator()<Const>();
			case Keyword::Var:
			case Keyword::Let:
				return visitor.template operator()<Variable>();
			case Keyword::Expand:
				return visitor.template operator()<Expand>();
			default:
				break;
			}

			if (lang_def::keywordFlags(as_keyword).contains(lang_def::KeywordFlagsOptions::IsAction))
				return visitor.template operator()<Action>();

			// Expr as stmt have semicolon at the end:
			return visitor.template operator()<ExprStmt>();
		}

		/**
		 * @brief Calls `visitor.template operator()<T>()` with the statement type chosen for the
		 * token `at` tokens ahead in the class statement context. `has_specifiers` tells whether
		 * the statement was preceded by specifiers.
		 */
		template<class Visitor>
		decltype(auto
		) visitClassStmtKind(LangParserState& state, i64 at, bool has_specifiers, Visitor& visitor) {
			if (has_specifiers && state[at].isBracketGroup(Token::BracketType::Curly))
				return visitor.template operator()<SpecifierBlock>();

			switch (state[at].asKeyword()) {
			case Keyword::Template:
				return visitor.template operator()<TemplateStmt>();
			case Keyword::Fun:
				return visitor.template operator()<Method>();
			// These are statements that are not class-specific when adding new ones be careful
			// about the fact that ContextStmt is set to Class here.
			case Keyword::Using:
				return visitor.template operator()<Using>();
			case Keyword::Class:
				return visitor.template operator()<Class>();
			default:
				break;
			}

			if (state[at].isStr(state.getContext()->class_name))
				return visitor.template operator()<ClassSpecial>();

			return visitor.template operator()<Field>();
		}

		/**
		 * @brief Calculates the heuristic length of any statement starting `base` tokens ahead,
		 * including its attributes and specifiers.
		 */
		u64 findAnyStatementLength(LangParserState& state, i64 base);

		template<>
		struct StmtFinder<TemplateStmt> {
			/**
			 * @brief The template header followed by the templated statement.
			 */
			static u64 findStatementLength(LangParserState& state, i64 base = 0) {
				i64 header_end = base + 1;
				if (state[header_end].isBracketGroup(Token::BracketType::Round)) header_end++;
				return base::safeIntConv<u64>(header_end - base)
				     + findAnyStatementLength(state, header_end);
			}
		};

		/**
		 * @brief Tokens taken by the attributes and specifiers of a statement.
		 */
		struct PrefixTokens {
			u64  length;
			bool has_specifiers;
		};

		/**
		 * @brief Heuristically counts the attribute and specifier tokens starting `base` tokens
		 * ahead, mirroring what Stmt::collectPrefixes consumes.
		 */
		PrefixTokens countPrefixTokens(LangParserState& state, i64 base) {
			i64  at             = base;
			bool has_specifiers = false;
			while (true) {
				if (state[at].is(Special::AtSign)) {
					at++;
					if (state[at].isIdentifier()) at++;
					while (state[at].is(NamedOperator::Period) && state[at + 1].isIdentifier())
						at += 2;
					if (state[at].is(NamedOperator::PeriodStar)) at++;
					if (state[at].isBracketGroup(Token::BracketType::Round)) at++;
				} else if (lang_def::keywordFlags(state[at].asKeyword())
				               .contains(lang_def::KeywordFlagsOptions::IsSpecifier)) {
					has_specifiers = true;
					if (StmtSpecifier::SPECIFIEIRS_CALL_LIST_REQUIRED.contains(state[at].asKeyword())
					    && state[at + 1].isBracketGroup(Token::BracketType::Round))
						at++;
					at++;
				} else {
					break;
				}
			}
			return { .length = base::safeIntConv<u64>(at - base), .has_specifiers = has_specifiers };
		}

		u64 findAnyStatementLength(LangParserState& state, i64 base) {
			auto prefix = countPrefixTokens(state, base);
			i64  at     = base + base::safeIntConv<i64>(prefix.length);
			auto length_of
				= [&]<class T>() -> u64 { return StmtFinder<T>::findStatementLength(state, at); };

			switch (state.getContext()->stmt_context) {
			case pst::StmtContext::Normal:
				return prefix.length + visitStmtKind(state, at, prefix.has_specifiers, length_of);
			case pst::StmtContext::Class:
				return prefix.length
				     + visitClassStmtKind(state, at, prefix.has_specifiers, length_of);
			default:
				CORE_PANIC("Undefined Stmt parsing context");
			}
		}

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
		auto prefix_tokens = internal::countPrefixTokens(state, 0);
		if (prefix_tokens.length > 0) fallbackLen(state, prefix_tokens.length);
		auto prefixes = collectPrefixes(state);
		if (prefix_tokens.length > 0) exitFallback(state);

		if (state[0].is(Special::Semicolon)
		    && (state[-1].is(Special::Semicolon) || internal::isSentinel(state, -1))) {
			state.tokens().skip();
			return nullptr;
		}

		if (state[0].is(Special::Semicolon) || internal::isSentinel(state, 0)) {
			state.logInt(base::makeBox<EmptyStatementError>(state.getPosition()));
			return nullptr;
		}

		bool has_specifiers = prefix_tokens.has_specifiers;
		auto parse_stmt = [&]<class T>() -> MBox<Stmt> { return internal::parseStmt<T>(state); };

		MBox<Stmt> out;
		// Parse Statement based on context
		switch (state.getContext()->stmt_context) {
		case pst::StmtContext::Normal:
			out = internal::visitStmtKind(state, 0, has_specifiers, parse_stmt);
			break;
		case pst::StmtContext::Class:
			out = internal::visitClassStmtKind(state, 0, has_specifiers, parse_stmt);
			break;
		default:
			CORE_PANIC("Undefined Stmt parsing context");
		}

		// Add Attributes and Specifiers
		if (out) out->addPrefixes(state, std::move(prefixes));

		PST_RETURN out;
	}

	MBox<Stmt> Stmt::parseSimpleExpr(LangParserState& state) {
		if (state[0].is(Special::Semicolon) || internal::isSentinel(state, 0)) {
			state.logInt(base::makeBox<EmptyStatementError>(state.getPosition()));
			return nullptr;
		}
		return internal::parseStmt<ExprStmt>(state);
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

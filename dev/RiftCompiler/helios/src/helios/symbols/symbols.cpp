#include "symbols.hpp"

#include <base/exceptions.hpp>
#include <base/string_id.hpp>
#include <base/stable_hashmap.hpp>
#include <base/stable_container.hpp>
#include <base/variant.hpp>

#include <query_framework/query_impl.hpp>
#include <pst_parser/elements/elements.hpp>

#include <vector>

#include "../lookup_result.hpp"
#include "../scope_symbol_id.hpp"
#include "../scopes/scopes.hpp"
#include "../pst_ref.hpp"

namespace compiler::helios {
	/**
	 * @TODO: move to some docs
	 *  * imports are just symbols that we will "lookup in" just like usings.
	 *    They will link to different modules.
	 *  * Scopes trees of different modules are independent to relax dependency
	 *
	 *  @TODO: what about lookup cycles -- we will need to probably refactor queries a bit
	 *  in the future
	 */


	/**
	 * @brief Query "linked-scope", that is scope
	 * that "lookup in" operation will perform lookup.
	 *
	 * @note For HELIOS internal use only
	 * @note It is a partial-Query. It won't work for all symbol
	 */
	DECLARE_QUERY(QueryLinkedScope, SymID, ScopeID);

	/**
	 * @brief Stores generic symbol data
	 */
	struct SymbolData {
		// created when creating SymbolData:
		ScopeID     scope;
		base::StrId name;
		bool        anonymous   = false;
		bool        is_wildcard = false;
		bool        is_alias    = false;
		bool        dependent   = false;
		SymbolKind  kind;

		// we will need to cast it:
		PstRef<pst::Stmt> pst_stmt;
	};

	/**
	 * @brief Helper struct used to access private SymID data.
	 */
	struct GetSymRef_Functor {
		static auto get(SymID id) { return id.ref; }
	};

	auto getSymRef(SymID id) { return GetSymRef_Functor::get(id); }

	bool isWildcard(SymID id) { return getSymRef(id)->is_wildcard; }

	base::StrId name(SymID id) { return getSymRef(id)->name; }

	SymbolKind kind(SymID id) { return getSymRef(id)->kind; }

	ScopeID scope(SymID id) { return getSymRef(id)->scope; }

	namespace {
		/**
		 * @brief Global Symbol Table
		 * @note: in the future it might not be needed once
		 * we will move toward more pure Query Model
		 */
		base::StableVector<SymbolData> symbol_table;

		template<class... T>
		auto putInSymtable(T&&... args) {
			auto key = symbol_table.emplaceBack(std::forward<T>(args)...);
			return symbol_table.getRef(key).value();
		}
	}

	/**
	 * @brief SymbolData Factory
	 *
	 * @param scope
	 * @param stmt
	 * @return base::borrow_ptr<SymbolData>
	 */
	base::borrow_ptr<SymbolData>
		makeSymbolFromStatement(const ScopeID& scope, PstRef<pst::Stmt> stmt) {
		switch (stmt->getKind()) {
		case pst::StmtKind::Fun: {
			auto&& function_ = dynamic_cast<const pst::Fun*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = function_->getName(),
				.kind     = SymbolKind::Function,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Namespace: {
			auto&& namespace_ = dynamic_cast<const pst::Namespace*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = namespace_->getName(),
				.kind     = SymbolKind::Namespace,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Const: {
			auto&& const_ = dynamic_cast<const pst::Const*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = const_->getName(),
				.kind     = SymbolKind::Const,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Struct: {
			auto&& struct_ = dynamic_cast<const pst::Struct*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = struct_->getName(),
				.kind     = SymbolKind::Struct,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Alias: {
			auto&& alias_ = dynamic_cast<const pst::Alias*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = alias_->getName(),
				.is_alias = true,
				.kind     = SymbolKind::Alias,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Using: {
			auto&& using_ = dynamic_cast<const pst::Using*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope = scope,
				.name
				= base::StrId(base::strConcat("<USING> ", using_->getPointed().front()).c_str()),
				.is_wildcard = true,
				.is_alias    = true,
				.kind        = SymbolKind::Using,
				.pst_stmt    = stmt,
			});
		}

		default:
			break;
		}
		RIFT_PANIC(
			base::strConcat("makeSymbolFromStatement bad symbol kind, stmt: ", typeid(stmt).name())
		);
	}

	struct IMPLEMENT_QUERY(QuerySymbolOfSTMT, SymID) {
		static auto provide(Context&, QKey key) -> PResult {
			return PResult{ makeSymbolFromStatement(key.scope, key.stmt) };
		}

		// @OPT: opt it?
		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolOfSTMT);

	struct IMPLEMENT_QUERY(QueryLookupInSymbol, LookupResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (key.symbol.ref->kind) {
			case SymbolKind::Using:
			case SymbolKind::Namespace: {
				auto linked_scope = ctx.query<QueryLinkedScope>(key.symbol);
				return ctx.query<QueryLookupInScope>(
					{ linked_scope, key.name, key.follow_wildcards }
				);
			}

			// @note: here case for variables will be calling TS
			default:
				throw base::NotYetImplemented("Lookup in symbol...");
			}
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInSymbol);

	struct LookupChainKey {
		std::vector<base::StrId> names;
		ScopeID                  begin_scope;
		bool                     follow_wildcards;
	};

	QUERY_EXTENSION(lookupChain, LookupChainKey, SymbolList);

	SymbolList lookupChain(query::Context& ctx, LookupChainKey key) {
		RIFT_ASSERT(key.names.size() > 0, "lookupDotted received zero names");

		// initial symbol:
		auto first = ctx.query<QueryLookupInScopeAndParents>(
			{ key.begin_scope, key.names[0], key.follow_wildcards }
		);

		if (not first.isSingle()) {
			// @TODO: error in state
			RIFT_PANIC("ambiguity in lookupChain");
		}

		if (key.names.size() == 1) return first.getAsSingle();

		SymbolList result = first.getAsSingle();

		for (usize i = 1; i < key.names.size(); i++) {
			auto append_res = ctx.query<QueryLookupInSymbol>(
				{ result.back(), key.names[i], key.follow_wildcards }
			);

			if (!append_res.isSingle()) {
				// @TODO: error in state
				RIFT_PANIC("ambiguity in lookup");
			}

			auto single_append_res = append_res.getAsSingle();
			result.insert(result.end(), single_append_res.begin(), single_append_res.end());
		}
		return result;
	}

	struct IMPLEMENT_QUERY(QueryLinkedScope, ScopeID) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (key.ref->kind) {
			case SymbolKind::Using: {
				auto using_stmt = dynamic_cast<const pst::Using*>(key.ref->pst_stmt.get());
				auto names      = using_stmt->getPointed();
				auto lookup_res
					= ctx.callExt<lookupChain>(LookupChainKey{ names, scope(key), false });
				RIFT_ASSERT(
					not lookup_res.empty(),
					"Using points to something that does not exists or is empty"
				);
				return ctx.query<QueryLinkedScope>({ lookup_res.back() });
			}
			case SymbolKind::Namespace: {
				auto namespace_stmt = dynamic_cast<const pst::Namespace*>(key.ref->pst_stmt.get());
				auto inner_scope    = ctx.query<QueryPrimaryCodeScopeFor>({
                    scope(key),
                    namespace_stmt->getBody(),
                });
				return inner_scope;
			}
			default:
				throw base::NotYetImplemented("Getting linked scope...");
			}
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLinkedScope);

	base::HashT KeyOf_QuerySymbolOfSTMT::customPerfectHash() const {
		auto hash_1 = base::perfectHash(scope);
		auto hash_2 = stmt->getID().asInt();

		// @FIXME: this does not work:
		return hash_1 * 143 + hash_2 * 7;
	}

	base::HashT KeyOf_LookupInSymbol::customPerfectHash() const {
		auto hash_1 = base::perfectHash(symbol);
		auto hash_2 = std::hash<base::StrId>()(name);

		// @FIXME: this does not work:
		return (hash_1 * 143 + hash_2 * 7) * 2 + follow_wildcards;
	}

	struct IMPLEMENT_QUERY(QueryDealias, SymbolList) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			if (kind(key) != SymbolKind::Alias) return { key };

			auto&& alias_definition
				= dynamic_cast<const pst::Alias*>(getSymRef(key)->pst_stmt.get());

			bool       first_symbol = true;
			SymbolList result;
			for (auto&& pointed: alias_definition->getPointed()) {
				auto&& pointed_symbol_lookup
					= first_symbol
				        ? ctx.query<QueryLookupInScopeAndParents>({ scope(key), pointed, false })
				        : ctx.query<QueryLookupInSymbol>({ result.back(), pointed, false });

				for (auto&& path = pointed_symbol_lookup.getAsSingle(); auto&& path_symbol: path) {
					auto&& dealiased = ctx.query<QueryDealias>(path_symbol);
					result.insert(result.end(), dealiased.begin(), dealiased.end());
				}

				first_symbol = false;
			}

			return result;
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDealias);

	int getPriority(const rpn::Operator& op) {
		switch (static_cast<char>(op.oper_id.view()[0])) {
		case '+':
		case '-':
			return 1;
		case '*':
		case '/':
		case '%':
			return 2;
		case '^':  // Power ;D
			return 3;
		case '.':
			return 5;
		default:
			RIFT_PANIC("Unknown operator: " + op.oper_id.str());
		}
	}

	std::vector<rpn::ExprElem>
		rpn::ExtensionMakeRPN(query::detail::ContextType& ctx, KeyOf_ExtensionMakeRPN key) {
		std::vector<ExprElem> rpn;
		std::stack<ExprElem>  st;
		for (auto&& e: key.expr) {
			variant_match(e) {
				variant_case(pst::Expr::Identifier, idt) {
					rpn.emplace_back(NamedIdentifier{ idt.indent_id });
				}
				variant_case(pst::Expr::Operator, oper) {
					auto      new_op   = Operator{ oper.oper_id };
					const int priority = getPriority(new_op);
					while (!st.empty() && getPriority(std::get<Operator>(st.top())) >= priority) {
						rpn.emplace_back(st.top());
						st.pop();
					}
					st.emplace(new_op);
				}
				variant_case(pst::Expr::NumLiteral, num) {
					rpn.emplace_back(NumValue{ num.num_id });
				}
				variant_case(pst::Expr::Group, group) {
					auto&& res = ctx.callExt<ExtensionMakeRPN>(KeyOf_ExtensionMakeRPN{
						group.expr->elements, key.expr_scope });
					rpn.insert(rpn.end(), res.begin(), res.end());
				}
				variant_case(pst::Expr::KeywordValue, keyword_val) {
					rpn.emplace_back(KeywordValue{ keyword_val.keyword });
				}
				variant_default { RIFT_PANIC("Bad Expr alternative"); }
			}
		}
		while (!st.empty()) {
			rpn.push_back(st.top());
			st.pop();
		}

		return rpn;
	}

	i32 rpn::ExtensionRPNValue(
		query::detail::ContextType& ctx, const KeyOf_ExtensionRPNValue& key
	) {
		variant_match(key.expr) {
			variant_case(rpn::Identifier, idt) {
				// .back() works for constants only.
				auto&& sym_list = ctx.query<QueryDealias>(idt.symbol_list.back());
				return ctx.query<QueryConstValueOf>(sym_list.back());
			}
			variant_case(rpn::NamedIdentifier, idt) {
				auto&& sym_list = ctx.query<QueryLookupInScopeAndParents>(
					{ key.expr_scope, idt.symbol_name, true }
				);
				return ctx.callExt<ExtensionRPNValue>(KeyOf_ExtensionRPNValue{
					Identifier{ sym_list.getAsSingle() },
					key.expr_scope,
				});
			}
			variant_case(rpn::Operator, op) { RIFT_PANIC("Cannot get a value from rpn::Operator"); }
			variant_case(rpn::KeywordValue, keyword_value) {
				throw base::NotYetImplemented("Value of KeywordValue is not yet implemented");
			}
			variant_case(rpn::NumValue, literal) { return std::stoi(literal.num_id.str()); }
			variant_default { RIFT_PANIC("Bad Expr alternative"); }
		}
		RIFT_PANIC("Error in RPNValue expr...");
	}

	rpn::ExprElem
		rpn::ExtensionRPNEval(query::detail::ContextType& ctx, const KeyOf_ExtensionRPNEval& key) {
		auto&& [a, op, b, expr_scope] = key;

		if (op.oper_id == ".") {
			// @TODO: Add a compiler log or some kind of information if lookup failes.
			SymbolList looked_up_symbol;
			variant_match(a) {
				variant_case(rpn::Identifier, idt) { looked_up_symbol = idt.symbol_list; }
				variant_case(rpn::NamedIdentifier, idt) {
					auto&& sym_list = ctx.query<QueryLookupInScopeAndParents>(
						{ key.expr_scope, idt.symbol_name, true }
					);
					looked_up_symbol = sym_list.getAsSingle();
				}
				variant_default {
					throw base::NotYetImplemented("Lookup on non-identifier is not yet implemented"
					);
				}
			}
			variant_match(b) {
				variant_case(rpn::NamedIdentifier, idt_right) {
					auto&& new_symbols = ctx.query<QueryLookupInSymbol>({
																			looked_up_symbol.back(),
																			idt_right.symbol_name,
																			true,
																		})
					                         .getAsSingle();
					looked_up_symbol.insert(
						looked_up_symbol.end(), new_symbols.begin(), new_symbols.end()
					);
					return Identifier{ looked_up_symbol };
				}
				variant_default {
					throw base::NotYetImplemented(
						"Lookup of other things than identifiers is not yet supported"
					);
				}
			}
			RIFT_PANIC("Something strange has happended during .operator evaluation...");
		}

		i32 a_value = ctx.callExt<ExtensionRPNValue>(KeyOf_ExtensionRPNValue{ a, expr_scope });
		i32 b_value = ctx.callExt<ExtensionRPNValue>(KeyOf_ExtensionRPNValue{ b, expr_scope });

		i32 value{};

		switch (static_cast<char>(op.oper_id.view()[0])) {
		case '+':
			value = a_value + b_value;
			break;
		case '-':
			value = a_value - b_value;
			break;
		case '*':
			value = a_value * b_value;
			break;
		case '/':
			value = a_value / b_value;
			break;
		case '%':
			value = a_value % b_value;
			break;
		case '^': {
			value = 1;
			while (b_value-- > 0) value *= a_value;
		} break;
		default:
			RIFT_PANIC("Unknown operator: " + op.oper_id.str());
		}
		return NumValue{ base::StrId(std::to_string(value).c_str()) };
	}

	struct IMPLEMENT_QUERY(QueryConstValueOf, i32) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			RIFT_ASSERT(kind(key) == SymbolKind::Const, "SymID is not a Const");

			auto    const_symbol = dynamic_cast<const pst::Const*>(getSymRef(key)->pst_stmt.get());
			ScopeID key_scope    = scope(key);

			const auto& expr = ctx.callExt<rpn::ExtensionMakeRPN>(rpn::KeyOf_ExtensionMakeRPN{
				const_symbol->getValue()->elements,
				key_scope,
			});

			// This is a nice RPN debug print.
			// std::cout << "RPN: \n";
			// for (auto&& e: expr) {
			// 	std::cout << "expr: ";
			// 	variant_match(e) {
			// 		variant_case(rpn::Identifier, idt) {
			// 			for (auto&& s: idt.symbol_list) std::cout << name(s).str() << '.';
			// 		}
			// 		variant_case(rpn::NamedIdentifier, idt) {
			// 			std::cout << idt.symbol_name.str() << '.';
			// 		}
			// 		variant_case(rpn::Operator, op) { std::cout << op.oper_id.str(); }
			// 		variant_case(rpn::KeywordValue, keyword_value) {
			// 			std::cout << keywordToStr(keyword_value.keyword).str();
			// 		}
			// 		variant_case(rpn::NumValue, literal) { std::cout << literal.num_id.str(); }
			// 		variant_default { RIFT_PANIC("Bad Expr alternative"); }
			// 	}
			// 	std::cout << '\n';
			// }

			// Here we will evaluate the RPN.
			std::stack<rpn::ExprElem> st;
			for (auto&& e: expr) {
				variant_match(e) {
					variant_case(rpn::Identifier, idt) {
						// Assuming idt is NOT A FUNCTION.
						st.emplace(idt);
					}
					variant_case(rpn::NamedIdentifier, idt) {
						// Assuming idt is NOT A FUNCTION.
						st.emplace(idt);
					}
					variant_case(rpn::Operator, oper) {
						const auto first = st.top();
						st.pop();
						const auto second = st.top();
						st.pop();

						st.push(ctx.callExt<rpn::ExtensionRPNEval>(rpn::KeyOf_ExtensionRPNEval{
							second,
							oper,
							first,
							key_scope,
						}));
					}
					variant_case(rpn::NumValue, num) { st.emplace(num); }
					variant_default { RIFT_PANIC("Bad Expr alternative"); }
				}
			}
			RIFT_ASSERT(st.size() == 1, "Expression stack should have 1 element");
			return ctx.callExt<rpn::ExtensionRPNValue>(rpn::KeyOf_ExtensionRPNValue{
				st.top(),
				key_scope,
			});
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryConstValueOf);
}

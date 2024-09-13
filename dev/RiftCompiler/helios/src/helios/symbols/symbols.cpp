#include "symbols.hpp"

#include <base/exceptions.hpp>
#include <base/string_id.hpp>
#include <base/stable_hashmap.hpp>
#include <base/stable_container.hpp>
#include <base/variant.hpp>
#include <base/unique_pointer.hpp>

#include <query_framework/query_impl.hpp>
#include <pst_parser/elements/elements.hpp>
#include <pst_parser/pst_visitor.hpp>
#include <helios/hout/elements.hpp>

#include <vector>

#include <typesystem/queries/types.hpp>
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

	PstRef<pst::Stmt> stmt(SymID id) { return getSymRef(id)->pst_stmt; }

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
	Ref<SymbolData> makeSymbolFromStatement(const ScopeID& scope, PstRef<pst::Stmt> stmt) {
		// @TODO: change this function to visitor to avoid dynamic_casts

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
		case pst::StmtKind::Class: {
			auto&& class_ = dynamic_cast<const pst::Class*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = class_->getName(),
				.kind     = SymbolKind::Class,
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
		case pst::StmtKind::Variable: {
			auto&& variable_ = dynamic_cast<const pst::Variable*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = variable_->getName(),
				.is_wildcard = false,
				.is_alias    = false,
				.kind        = SymbolKind::Variable,
				.pst_stmt    = stmt,
			});
		}
		case pst::StmtKind::Import: {
			// For now only non-wildcard import exist
			auto&& import = dynamic_cast<const pst::Import*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = import->getAlias(),
				.is_wildcard = false,
				.is_alias    = false,
				.kind        = SymbolKind::Import,
				.pst_stmt    = stmt,
			});
		}
		case pst::StmtKind::Method: {
			auto&& method = dynamic_cast<const pst::Method*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = method->getName(),
				.kind     = SymbolKind::Method,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Field: {
			auto&& field = dynamic_cast<const pst::Field*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = field->getName(),
				.kind     = SymbolKind::Field,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Constructor: {
			auto&& constructor = dynamic_cast<const pst::Constructor*>(stmt.get());
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = constructor->getName(),
				.kind     = SymbolKind::Constructor,
				.pst_stmt = stmt,
			});
		}
		case pst::StmtKind::Destructor: {
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = base::StrId("destroy"),
				.kind     = SymbolKind::Destructor,
				.pst_stmt = stmt,
			});
		}
		default:
			break;
		}
		auto stmt_ptr = stmt.get();
		RIFT_PANIC(base::strConcat(
			"makeSymbolFromStatement bad symbol kind, stmt: ", typeid(*stmt_ptr).name()
		));
	}

	struct IMPLEMENT_QUERY(QuerySymbolOfSTMT, SymID) {
		static auto provide(Context&, QKey key) -> PResult {
			return PResult{ makeSymbolFromStatement(key.scope, key.stmt) };
		}

		// @OPT: opt it?
		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolOfSTMT);

	struct IMPLEMENT_QUERY(QueryLookupInSymbol, QueryLookup_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (key.symbol.ref->kind) {
			case SymbolKind::Using:
			case SymbolKind::Namespace:
			case SymbolKind::Import: {
				// @NOTE: for now imports are done via linked scope that looks at root
				// module scope, but in the future it might be changed to custom code

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

	/**
	 * @brief Query extension for looking-up chain of names
	 */
	std::expected<SymbolList, std::variant<errors::AmbiguityError, QueryLookup_Result::error_type>>
		lookupChain(query::Context& ctx, const LookupChainKey& key) {
		RIFT_ASSERT(!key.names.empty(), "lookupDotted received zero names");

		// initial symbol:
		UNPACK_RESULT(
			ctx.query<QueryLookupInScopeAndParents>(
				{ key.begin_scope, key.names[0], key.follow_wildcards }
			),
			first
		);

		if (not first.isSingle()) {
			// @TODO: error in state
			return std::unexpected(errors::AmbiguityError("ambiguity in lookupChain"));
		}

		if (key.names.size() == 1) return first.getAsSingle();

		UNPACK_RESULT(first.getAsSingle(), result);

		for (usize i = 1; i < key.names.size(); i++) {
			UNPACK_RESULT(
				ctx.query<QueryLookupInSymbol>({ result.back(), key.names[i], key.follow_wildcards }
			    ),
				append_res_value
			);

			if (!append_res_value.isSingle()) {
				// @TODO: error in state
				return std::unexpected(errors::AmbiguityError("ambiguity in lookupChain"));
			}

			UNPACK_RESULT(append_res_value.getAsSingle(), single_append_res);
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
				auto lookup_res = lookupChain(ctx, LookupChainKey{ names, scope(key), false });
				RIFT_ASSERT(
					lookup_res.has_value() && not lookup_res.value().empty(),
					"Using points to something that does not exists or is empty"
				);
				return ctx.query<QueryLinkedScope>({ lookup_res.value().back() });
			}
			case SymbolKind::Namespace: {
				auto namespace_stmt = dynamic_cast<const pst::Namespace*>(key.ref->pst_stmt.get());
				auto inner_scope    = ctx.query<QueryPrimaryCodeScopeFor>({
                    namespace_stmt->getBody(),
                });
				return inner_scope;
			}
			case SymbolKind::Import: {
				auto import_stmt = dynamic_cast<const pst::Import*>(key.ref->pst_stmt.get());

				// @TODO: proper error handling via ErrorScope
				auto imported_module = frontend::getRelativeModule(
										   ctx, module(scope(key)), import_stmt->getModulePath()
				)
				                           .value();

				// Here we don't access just root scope, because root scopes are currently empty:
				auto linked_scope = extendQueryRootScopeOfMainModuleFile(ctx, imported_module);

				return linked_scope;
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

	struct
		IMPLEMENT_QUERY(QueryDealias, std::expected<SymbolList COMMA QueryLookup_Result::error_type>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			if (kind(key) != SymbolKind::Alias) return SymbolList{ key };

			auto&& alias_definition
				= dynamic_cast<const pst::Alias*>(getSymRef(key)->pst_stmt.get());

			bool       first_symbol = true;
			SymbolList result;
			for (auto&& pointed: alias_definition->getPointed()) {
				UNPACK_RESULT(
					first_symbol
						? ctx.query<QueryLookupInScopeAndParents>({ scope(key), pointed, false })
						: ctx.query<QueryLookupInSymbol>({ result.back(), pointed, false }),
					pointed_symbol_lookup
				);
				UNPACK_RESULT(pointed_symbol_lookup.getAsSingle(), path);
				for (auto&& path_symbol: path) {
					UNPACK_RESULT(ctx.query<QueryDealias>(path_symbol), dealiased);
					result.insert(result.end(), dealiased.begin(), dealiased.end());
				}

				first_symbol = false;
			}

			return result;
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDealias);

	std::expected<i32, errors::ExpressionParsingError> getPriority(const rpn::Operator& op) {
		switch (static_cast<char>(op.oper_id.view()[0])) {
		case '+':
		case '-':
			return 1;
		case '*':
		case '/':
		case '%':
		case '|':
			return 2;
		case '^':  // Power ;D
			return 3;
		case '.':
			return 5;
		default:
			HELIOS_PANIC(errors::ExpressionParsingError, "Unknown operator: " + op.oper_id.str());
		}
	}

	rpn::MakeRPN_Result rpn::makeRPN(query::Context& ctx, KeyOf_RPNmakeRPN key) {
		std::vector<ExprElem> rpn;
		std::stack<ExprElem>  st;
		for (auto&& e: key.expr) {
			variant_match(e) {
				variant_case(pst::Expr::Identifier, idt) {
					rpn.emplace_back(NamedIdentifier{ idt.indent_id });
				}
				variant_case(pst::Expr::Operator, oper) {
					auto new_op = Operator{ oper.oper_id };
					UNPACK_RESULT(getPriority(new_op), priority);
					while (!st.empty()) {
						UNPACK_RESULT(getPriority(std::get<Operator>(st.top())), top_priority);
						if (top_priority < priority) break;
						rpn.emplace_back(st.top());
						st.pop();
					}
					st.emplace(new_op);
				}
				variant_case(pst::Expr::NumLiteral, num) {
					rpn.emplace_back(NumValue{ num.num_id });
				}
				variant_case(pst::Expr::Group, group) {
					UNPACK_RESULT(
						makeRPN(ctx, KeyOf_RPNmakeRPN{ group.expr->elements, key.expr_scope }), res
					);
					rpn.insert(rpn.end(), res.elements.begin(), res.elements.end());
				}
				variant_case(pst::Expr::KeywordValue, keyword_val) {
					rpn.emplace_back(KeywordValue{ keyword_val.keyword });
				}
				variant_case(pst::Expr::CommaSeparated, tuple) {
					for (auto&& type_expr: tuple.expr) {
						UNPACK_RESULT(
							makeRPN(ctx, KeyOf_RPNmakeRPN{ type_expr->elements, key.expr_scope }),
							res
						);
						rpn.insert(rpn.end(), res.elements.begin(), res.elements.end());
					}
					rpn.emplace_back(TupleConstructor{ tuple.expr.size() });
				}
				variant_default {
					HELIOS_PANIC(errors::ExpressionParsingError, "Bad Expr alternative");
				}
			}
		}
		while (!st.empty()) {
			rpn.push_back(st.top());
			st.pop();
		}

		return RPNExpr{ rpn, key.expr_scope };
	}

	QueryConstValueOf_Result rpn::parseValue(query::Context& ctx, const KeyOf_parseValue& key) {
		variant_match(key.expr) {
			variant_case(rpn::Identifier, idt) {
				// .back() works for constants only.
				UNPACK_RESULT(ctx.query<QueryDealias>(idt.symbol_list.back()), sym_list);
				return ctx.query<QueryConstValueOf>(sym_list.back());
			}
			variant_case(rpn::NamedIdentifier, idt) {
				UNPACK_RESULT(
					ctx.query<QueryLookupInScopeAndParents>(
						{ key.expr_scope, idt.symbol_name, true }
					),
					sym_list
				);

				UNPACK_RESULT(sym_list.getAsSingle(), identifier);
				UNPACK_RESULT(
					parseValue(
						ctx,
						KeyOf_parseValue{
							Identifier{ identifier },
							key.expr_scope,
						}
					),
					result
				);
				return result;
			}
			variant_case(rpn::Operator, op) {
				HELIOS_PANIC(
					errors::ExpressionParsingError, "Cannot get a value from rpn::Operator"
				);
			}
			variant_case(rpn::KeywordValue, keyword_value) {
				throw base::NotYetImplemented("Value of KeywordValue is not yet implemented");
			}
			variant_case(rpn::NumValue, literal) { return std::stoi(literal.num_id.str()); }
			variant_default {
				HELIOS_PANIC(errors::ExpressionParsingError, "Bad Expr alternative");
			}
		}
		HELIOS_PANIC(errors::ExpressionParsingError, "Error in RPNValue expr...");
	}

	namespace {
		void printRpn(const rpn::RPNExpr& expr) {
			// This is a nice RPN debug print.
			std::cerr << "RPN: \n";
			for (auto&& e: expr.elements) {
				std::cerr << "expr: ";
				variant_match(e) {
					variant_case(rpn::Identifier, idt) {
						for (auto&& s: idt.symbol_list) std::cerr << name(s).str() << '.';
					}
					variant_case(rpn::NamedIdentifier, idt) {
						std::cerr << idt.symbol_name.str() << '.';
					}
					variant_case(rpn::Operator, op) { std::cerr << op.oper_id.str(); }
					variant_case(rpn::KeywordValue, keyword_value) {
						std::cerr << keywordToStr(keyword_value.keyword).str();
					}
					variant_case(rpn::NumValue, literal) { std::cerr << literal.num_id.str(); }
					variant_case(rpn::TupleConstructor, tuple_constructor) {
						std::cerr << "Tuple constructor of num elements: "
								  << tuple_constructor.num_elements;
					}
					variant_default { RIFT_PANIC("Bad Expr alternative"); }
				}
				std::cerr << '\n';
			}
		}
	}

	rpn::RPNEvaluation_Result rpn::evalExpr(query::Context& ctx, const RPNExpr& expr) {
		printRpn(expr);
		// Here we will evaluate the RPN.
		std::stack<ExprElem> st;
		for (auto&& e: expr.elements) {
			variant_match(e) {
				variant_case(rpn::Identifier, idt) {
					// Assuming idt is NOT A FUNCTION.
					st.emplace(idt);
				}
				variant_case(rpn::NamedIdentifier, idt) {
					// Assuming idt is NOT A FUNCTION.
					st.emplace(idt);
				}
				variant_case(rpn::KeywordValue, keyword) { st.emplace(keyword); }
				variant_case(rpn::Operator, oper) {
					HELIOS_ASSERT(
						st.size() >= 2, errors::ExpressionParsingError, "Malformed expression"
					);
					const auto first = st.top();
					st.pop();
					const auto second = st.top();
					st.pop();

					UNPACK_RESULT(
						evalOperator(
							ctx,
							KeyOf_evalOperator{
								second,
								oper,
								first,
								expr.scope,
							}
						),
						evaluated
					);
					st.push(evaluated);
				}
				variant_case(rpn::NumValue, num) { st.emplace(num); }
				variant_case(rpn::TupleConstructor, tuple) {
					TupleType tuple_type;
					for (usize i = 0; i < tuple.num_elements; i++) {
						RIFT_ASSERT(
							!st.empty(), "Logic error during tuple creation, not enough elements"
						);
						auto tuple_element = st.top();
						st.pop();
						tuple_type.elements.push_back(tuple_element);
					}
					st.emplace(tuple_type);
				}
				variant_default {
					HELIOS_PANIC(errors::ExpressionParsingError, "Bad Expr alternative");
				}
			}
		}
		RIFT_ASSERT(st.size() == 1, "Expression stack should have 1 element");
		return st.top();
	}

	rpn::RPNEvaluation_Result
		rpn::evalOperator(query::Context& ctx, const KeyOf_evalOperator& key) {
		auto [a, op, b, expr_scope] = key;

		if (op.oper_id == ".") {
			// @TODO: Add a compiler log or some kind of information if lookup failes.
			SymbolList looked_up_symbol;
			variant_match(a) {
				variant_case(rpn::Identifier, idt) { looked_up_symbol = idt.symbol_list; }
				variant_case(rpn::NamedIdentifier, idt) {
					UNPACK_RESULT(
						ctx.query<QueryLookupInScopeAndParents>(
							{ key.expr_scope, idt.symbol_name, true }
						),
						sym_list
					);
					UNPACK_RESULT(sym_list.getAsSingle(), new_symbol);
					looked_up_symbol = new_symbol;
				}
				variant_default {
					throw base::NotYetImplemented("Lookup on non-identifier is not yet implemented"
					);
				}
			}
			variant_match(b) {
				variant_case(rpn::NamedIdentifier, idt_right) {
					UNPACK_RESULT(
						ctx.query<QueryLookupInSymbol>({
							looked_up_symbol.back(),
							idt_right.symbol_name,
							true,
						}),
						new_symbols
					);
					UNPACK_RESULT(new_symbols.getAsSingle(), single);
					looked_up_symbol.insert(looked_up_symbol.end(), single.begin(), single.end());
					return Identifier{ looked_up_symbol };
				}
				variant_default {
					throw base::NotYetImplemented(
						"Lookup of other things than identifiers is not yet supported"
					);
				}
			}
			HELIOS_PANIC(
				errors::ExpressionParsingError,
				"Something strange has happended during .operator evaluation..."
			);
		}
		if (op.oper_id == "|") {
			// @TODO: Check if A and B are types.

			static auto is_variant
				= [](auto&& expr_elem) { return std::holds_alternative<Variant>(expr_elem); };

			const auto is_variant_a = is_variant(a);
			const auto is_variant_b = is_variant(b);

			if (!(is_variant_a || is_variant_b)) {
				// If neither A nor B are variants, then create a new variant type
				// with two types: A and B
				return Variant{ { a, b } };
			}

			// Now, `a` will be a variant.
			// @TODO: https://github.com/ducktype-org/rift-dev/pull/169#discussion_r1654601995
			if (is_variant_b) std::swap(a, b);

			variant_match(a) {
				variant_case(rpn::Variant, a_variant) {
					variant_match(b) {
						variant_case(rpn::Variant, b_variant) {
							a_variant.elements.insert(
								a_variant.elements.end(),
								b_variant.elements.begin(),
								b_variant.elements.end()
							);
						}
						variant_default { a_variant.elements.push_back(b); }
					}
				}
				variant_default {
					HELIOS_PANIC(
						errors::ExpressionParsingError, "A is not a variant, but it should be."
					);
				}
			}
			return a;
		}


		UNPACK_RESULT(parseValue(ctx, KeyOf_parseValue{ a, expr_scope }), a_value);
		UNPACK_RESULT(parseValue(ctx, KeyOf_parseValue{ b, expr_scope }), b_value);

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

	struct IMPLEMENT_QUERY(QueryConstValueOf, QueryConstValueOf_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			RIFT_ASSERT(kind(key) == SymbolKind::Const, "SymID is not a Const");

			const auto const_symbol
				= dynamic_cast<const pst::Const*>(getSymRef(key)->pst_stmt.get());
			const ScopeID key_scope = scope(key);

			UNPACK_RESULT(
				rpn::makeRPN(
					ctx,
					{
						const_symbol->getValue()->elements,
						key_scope,
					}
				),
				expr
			);
			UNPACK_RESULT(rpn::evalExpr(ctx, expr), evaluated);
			return parseValue(
				ctx,
				rpn::KeyOf_parseValue{
					evaluated,
					key_scope,
				}
			);
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryConstValueOf);

	/**
	 * Parses the expression assuming it evaluates to a type. Panics otherwise.
	 * @param ctx Context passed to a query.
	 * @param expr An RPN expression created with e.g. ExtensionMakeRPN.
	 * @param expr_scope A scope where the expression has been expressed.
	 * @return ts::TypeInfo with information about the evaluated type.
	 */
	ParseTypeFromExpr_Result
		parseTypeFromExpr(query::Context& ctx, const rpn::ExprElem& expr, ScopeID expr_scope) {
		// This is a std::unordered_map, not base::HashMap, because base::HashMap
		// does not support this constructor.
		const static auto BUILTINS = std::unordered_map<base::StrId, ts::TypeInfo>{
			{ base::StrId("f128"), ctx.query<::ts::QueryFloatType>(128) },
			{ base::StrId("f80"), ctx.query<::ts::QueryFloatType>(80) },
			{ base::StrId("f64"), ctx.query<::ts::QueryFloatType>(64) },
			{ base::StrId("f32"), ctx.query<::ts::QueryFloatType>(32) },
			{ base::StrId("f16"), ctx.query<::ts::QueryFloatType>(16) },

			{ base::StrId("i128"), ctx.query<::ts::QueryIntegralType>({ 128, true }) },
			{ base::StrId("i64"), ctx.query<::ts::QueryIntegralType>({ 64, true }) },
			{ base::StrId("i32"), ctx.query<::ts::QueryIntegralType>({ 32, true }) },
			{ base::StrId("i16"), ctx.query<::ts::QueryIntegralType>({ 16, true }) },
			{ base::StrId("i8"), ctx.query<::ts::QueryIntegralType>({ 8, true }) },

			{ base::StrId("u128"), ctx.query<::ts::QueryIntegralType>({ 128, false }) },
			{ base::StrId("u64"), ctx.query<::ts::QueryIntegralType>({ 64, false }) },
			{ base::StrId("u32"), ctx.query<::ts::QueryIntegralType>({ 32, false }) },
			{ base::StrId("u16"), ctx.query<::ts::QueryIntegralType>({ 16, false }) },
			{ base::StrId("u8"), ctx.query<::ts::QueryIntegralType>({ 8, false }) },
		};
		variant_match(expr) {
			variant_case(rpn::Identifier, idt) {
				return ctx.query<QueryTypeOfSymbol>(idt.symbol_list.back());
			}
			variant_case(rpn::Operator, oper) {
				HELIOS_PANIC(
					errors::ExpressionParsingError,
					base::strConcat("Type cannot be an operator: ", oper.oper_id)
				);
			}
			variant_case(rpn::NamedIdentifier, named_identifier) {
				const auto it = BUILTINS.find(named_identifier.symbol_name);
				if (it == BUILTINS.end()) {
					UNPACK_RESULT(
						ctx.query<QueryLookupInScopeAndParents>({
							expr_scope,
							named_identifier.symbol_name,
							true,
						}),
						symbol
					);
					return ctx.query<QueryTypeFromDefinition>(symbol.leaves.back());
				}
				return it->second;
			}
			variant_case(rpn::KeywordValue, keyword_val) {
				const auto it = BUILTINS.find(keywordToStr(keyword_val.keyword));
				if (it == BUILTINS.end()) {
					throw base::NotYetImplemented(strConcat(
						"KeywordValue is not yet handled by the QueryTypeOfSymbol: ",
						keywordToStr(keyword_val.keyword)
					));
				}
				return it->second;
			}
			variant_case(rpn::NumValue, num_value) {
				RIFT_PANIC("Numerical value is not a type: ", num_value.num_id);
			}
			variant_case(rpn::TupleType, tuple_type) {
				std::vector<ts::ComponentType> tuple_components;
				tuple_components.reserve(tuple_type.elements.size());

				for (auto&& tuple_subtype: tuple_type.elements) {
					UNPACK_RESULT(
						parseTypeFromExpr(ctx, tuple_subtype, expr_scope), parsed_subtype
					);
					tuple_components.emplace_back(parsed_subtype, false);
				}
				std::reverse(tuple_components.begin(), tuple_components.end());

				return ctx.query<ts::QueryTupleType>({ tuple_components });
			}
			variant_case(rpn::Variant, variant_type) {
				std::vector<ts::TypeInfo> variant_types;

				variant_types.reserve(variant_type.elements.size());
				for (auto&& variant_subtype: variant_type.elements) {
					UNPACK_RESULT(
						parseTypeFromExpr(ctx, variant_subtype, expr_scope), parsed_variant_subtype
					);
					variant_types.push_back(parsed_variant_subtype);
				}

				return ctx.query<ts::QueryVariantType>({ variant_types });
			}
			variant_default { RIFT_PANIC("Unhandleble type during parsing type from expr..."); }
		}
		HELIOS_PANIC(errors::ExpressionParsingError);
		RIFT_PANIC("Couldn't parse the type.");
	}

	/**
	 * Parses the expression assuming it evaluates to a type. Panics otherwise.
	 * @param ctx Context passed to a query.
	 * @param expr A PST expression, that has been written in the source code.
	 * @param expr_scope A scope where the expression has been expressed.
	 * @return ts::TypeInfo with information about the evaluated type.
	 */
	ParseTypeFromExpr_Result parseTypeFromExpr(
		query::Context& ctx, const tpc::ParserCBorrowRef<pst::Expr>& expr, ScopeID expr_scope
	) {
		UNPACK_RESULT(rpn::makeRPN(ctx, { expr->elements, expr_scope }), rpn_expr);
		UNPACK_RESULT(rpn::evalExpr(ctx, rpn_expr), final_type);
		UNPACK_RESULT(parseTypeFromExpr(ctx, final_type, expr_scope), result);
		return result;
	}

	struct IMPLEMENT_QUERY(QueryTypeOfSymbol, ParseTypeFromExpr_Result) {
		class PstStmtVisitor_GetTypeOf final: public pst::PstStmtVisitorPanicky {
			Context&    ctx;
			const QKey& key;

			void setTypeOfSymbol(const ParseTypeFromExpr_Result& type) {
				if (symbol_type_info.has_value())
					RIFT_PANIC("Attempted to set type of symbol in visitor a second time.");
				symbol_type_info = type;
			}

			void setTypeOfSymbol(const pst::ParserCBorrowRef<pst::Expr>& expr) {
				setTypeOfSymbol(parseTypeFromExpr(ctx, expr, scope(key)));
			}

		public:
			PstStmtVisitor_GetTypeOf(Context& ctx, const QKey& key): ctx(ctx), key(key) {}

			base::Optional<ParseTypeFromExpr_Result> symbol_type_info;

			void visitConst(const pst::Const& stmt) override { setTypeOfSymbol(stmt.getType()); }

			void visitVariable(const pst::Variable& stmt) override {
				setTypeOfSymbol(stmt.getType());
			}

			void visitField(const pst::Field& field) override { setTypeOfSymbol(field.getType()); }

			void visitFun(const pst::Fun& fun) override {
				auto params = fun.getParams();
				auto ret    = fun.getRet();

				std::vector<ts::TypeInfo> param_types{};
				param_types.reserve(params->size());

				for (auto param: params) {
					auto&& parse_type_res = parseTypeFromExpr(ctx, param->getType(), scope(key));
					if (parse_type_res.has_value()) {
						param_types.emplace_back(parse_type_res.value());
					} else {
						// In this case we are setting error
						setTypeOfSymbol(parse_type_res);
						return;
					}
				}
				ts::TypeInfo ret_type = ctx.query<ts::QueryUnitType>({});
				if (ret.has_value()) {
					auto&& parsed = parseTypeFromExpr(ctx, ret.value(), scope(key));
					if (parsed.has_value()) {
						ret_type = parsed.value();
					} else {
						// In this case we are setting error
						setTypeOfSymbol(parsed);
						return;
					}
				}

				setTypeOfSymbol(ctx.query<ts::QueryFunctionType>({ param_types, ret_type }));
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto&& symbol_ref = getSymRef(key);

			PstStmtVisitor_GetTypeOf visitor(ctx, key);
			symbol_ref->pst_stmt->acceptVisitor(visitor);
			return visitor.symbol_type_info.value();
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeOfSymbol);

	struct IMPLEMENT_QUERY(QueryTypeFromDefinition, ParseTypeFromExpr_Result) {
		class PstStmtVisitor_GetTypeFromDefinition final: public pst::PstStmtVisitorPanicky {
			Context&    ctx;
			const QKey& key;

			void setTypeOfDefinition(const ts::TypeInfo& type) {
				if (definition_type_info.has_value())
					RIFT_PANIC("Attempted to set type of definition in visitor a second time.");
				definition_type_info = type;
			}

		public:
			PstStmtVisitor_GetTypeFromDefinition(Context& ctx, const QKey& key):
				  ctx(ctx),
				  key(key) {}

			base::Optional<ts::TypeInfo> definition_type_info;

			void visitClass(const pst::Class&) override {
				definition_type_info = ctx.query<ts::QueryClassType>(key);
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto&& symbol_ref = getSymRef(key);

			PstStmtVisitor_GetTypeFromDefinition visitor(ctx, key);
			symbol_ref->pst_stmt->acceptVisitor(visitor);
			return visitor.definition_type_info.value();
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF;
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeFromDefinition)

	struct IMPLEMENT_QUERY(QueryClassSymbolData, QueryClassSymbolData_Result) {
		struct ClassDataParser final: pst::PstStmtVisitorPanicky {
			base::Optional<base::StrId>                                name;
			base::Optional<tpc::ParserCBorrowRef<pst::Expr>>           base_class;
			base::Optional<tpc::ParserCBorrowRef<pst::ImplementsList>> implements;

			void visitClass(const pst::Class& stmt) override {
				name = stmt.getName();
				if (auto&& base = stmt.getBase(); base != nullptr) base_class = base;
				if (auto&& implements = stmt.getImplements(); implements != nullptr)
					this->implements = implements;
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			RIFT_ASSERT(kind(key) == SymbolKind::Class, "Symbol is not a class");

			auto class_stmt = getSymRef(key)->pst_stmt;

			auto&& class_scope   = ctx.query<QueryPrimaryCodeScopeFor>({ class_stmt });
			auto&& class_symbols = ctx.query<QuerySymbolsInScope>(class_scope);

			ClassSymbolData class_info;
			for (auto&& sym: class_symbols) {
				switch (kind(sym)) {
				case SymbolKind::Method:
					class_info.methods.push_back(sym);
					break;
				case SymbolKind::Constructor:
					class_info.constructors.push_back(sym);
					break;
				case SymbolKind::Destructor:
					// This doesn't catch multiple destructors
					class_info.destructor = sym;
					break;
				case SymbolKind::Field:
					class_info.members.push_back(sym);
					break;
				default:
					throw base::NotYetImplemented(base::strConcat(
						"Using ",
						typeid(kind(sym)).name(),
						" inside a class is not yet implemented."
					));
				}
			}
			// Find the name
			auto class_data_parser = ClassDataParser();
			class_stmt->acceptVisitor(class_data_parser);
			class_info.name = class_data_parser.name.value();

			if_opt_some(class_data_parser.base_class, base) {
				UNPACK_RESULT(parseTypeFromExpr(ctx, base, scope(key)), parsed);
				class_info.base = parsed;
			}

			if_opt_some(class_data_parser.implements, implements) {
				for (auto&& interface: implements) {
					UNPACK_RESULT(parseTypeFromExpr(ctx, interface, scope(key)), parsed);
					class_info.implements.push_back(parsed);
				}
			}

			return class_info;
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassSymbolData);

	struct
		IMPLEMENT_QUERY(QueryHOUTExprTreeOfSym, std::expected<base::unique_ptr<code::Expr> COMMA rpn::MakeRPN_Result::error_type>) {
		class PstStmtVisitor_GetHOUTExprTree final: public pst::PstStmtVisitorPanicky {
			Context&    ctx;
			const QKey& key;

			void setExprTree(const pst::ParserCBorrowRef<pst::Expr>& expr) {
				rpn_of_sym_expr = rpn::makeRPN(ctx, { expr->elements, scope(key) });
			}

		public:
			PstStmtVisitor_GetHOUTExprTree(Context& ctx, const QKey& key): ctx(ctx), key(key) {}

			base::Optional<rpn::MakeRPN_Result> rpn_of_sym_expr;

			void visitConst(const pst::Const& stmt) override { setExprTree(stmt.getValue()); }

			void visitVariable(const pst::Variable& stmt) override { setExprTree(stmt.getType()); }
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto&& symbol_ref = getSymRef(key);

			PstStmtVisitor_GetHOUTExprTree visitor(ctx, key);
			symbol_ref->pst_stmt->acceptVisitor(visitor);
			RIFT_ASSERT(
				visitor.rpn_of_sym_expr.has_value(),
				"Something wrong has happened while parsing expr"
			);
			UNPACK_RESULT(visitor.rpn_of_sym_expr.value(), rpn_expr);
			return code::Expr::fromRPN(ctx, { rpn_expr.elements, scope(key) });
		}

		static inline base::
			HashMap<QKey, query::CacheEntry<PResult>, ::base::PerfectHashFunctor<QKey>>
				cache;

		static auto load(const QKey& key) -> LoadResult {
			if (auto&& copy = cache.atMaybe(key)) {
				if (copy->data.has_value())
					return QResWithACD{ copy->data->borrow(), copy->acd };
				else
					return QResWithACD{ std::unexpected(copy->data.error()), copy->acd };
			}
			return {};
		}

		static auto store(const QKey& key, PResult res, query::ACD) -> QResult {
			cache.put(key, std::move(res));

			if (auto&& c = cache.at(key).data; c.has_value())
				return c.value().borrow();
			else
				return std::unexpected(c.error());
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHOUTExprTreeOfSym);
}

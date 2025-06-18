#include "symbols.hpp"

#include <frontend/module_tree/queries.hpp>
#include <helios_private/comp_time/int_eval.hpp>
#include <helios_private/comp_time/type_eval.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup_chain.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <pst_parser/pst_visitor.hpp>
#include <query_framework/query_impl.hpp>
#include <typesystem/higher/abstract_type.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <base/stable_container.hpp>
#include <base/stable_hashmap.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vector>

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

	bool isWildcard(SymID id) { return getSymRef(id)->common.is_wildcard; }

	base::StrID name(SymID id) { return getSymRef(id)->common.name; }

	SymbolKind kind(SymID id) { return getSymRef(id)->common.kind; }

	ScopeID scope(SymID id) { return getSymRef(id)->getPSTData()->scope; }

	base::Optional<ScopeID> maybeScope(SymID id) {
		variant_match(getSymRef(id)->other) {
			variant_case(PstSymbolData, pst_data) { return pst_data.scope; }
			variant_case_novalue(builtin::BuiltinFunctionData) { return base::Optional<ScopeID>{}; }
			variant_default { CORE_PANIC("Unhandled symbol kind"); }
		}
		CORE_UNREACHABLE();
	}

	base::Optional<pst::Access<pst::Stmt>> stmt(query::Context& ctx, SymID id) {
		return getSymRef(id)->stmtCast(ctx);
	}

	pst::AccessLocked<pst::LangElement> symbolPst(SymID id) {
		return getSymRef(id)->getPSTData()->pst_element;
	}

	namespace {
		/**
		 * @brief Global Symbol Table
		 * @note: in the future it might not be needed once
		 * we will move toward more pure Query Model
		 */
		base::StableVector<const SymbolData> symbol_table;

		template<class... T>
		CRef<SymbolData> putInSymtable(T&&... args) {
			symbol_table.emplaceBack(std::forward<T>(args)...);
			return symbol_table.last();
		}
	}

	std::vector<SymID> getAllHeliosSymbols() {
		CORE_ASSERT(
			query::Context::getState().queryStackSize() == 0,
			"getAllHeliosSymbols called from within query!"
		);
		std::vector<SymID> output;
		output.reserve(symbol_table.size());
		for (auto& symbol: symbol_table) output.push_back(GetSymRef_Functor::make(symbol.ref()));
		return output;
	}

	/**
	 * @brief SymbolData Factory.
	 * Make symbols from PST statements.
	 * @todo in the future this function should not use dynamic_casts,
	 * and should be merged with makeSymbolFromPSTElement.
	 * It should use a visitor, or only depend on StmtKind and virtual methods.
	 *
	 * @param scope
	 * @param stmt
	 * @return Ref<SymbolData>
	 */
	CRef<SymbolData> makeSymbolFromStatement(
		query::Context& ctx, ScopeID scope, pst::Access<pst::Stmt> stmt
	) {
		// @TODO: change this function to visitor to avoid dynamic_casts

		PstSymbolData pst_data{
			.scope       = scope,
			.pst_element = stmt,
		};

		switch (stmt->getStmtKind()) {
		case pst::StmtKind::Fun: {
			auto function = stmt.dynamicCast<pst::Fun>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name = function->getName(),
					.kind = SymbolKind::Function,
				},
				pst_data
			));
		}
		case pst::StmtKind::Namespace: {
			auto namespace_stmt = stmt.dynamicCast<pst::Namespace>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name = namespace_stmt->getName(),
					.kind = SymbolKind::Namespace,
				},
				pst_data
			));
		}
		case pst::StmtKind::Const: {
			auto const_stmt = stmt.dynamicCast<pst::Const>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name = const_stmt->getName(),
					.kind = SymbolKind::Const,
				},
				pst_data
			));
		}
		case pst::StmtKind::Class: {
			auto class_stmt = stmt.dynamicCast<pst::Class>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name = class_stmt->getName(),
					.kind = SymbolKind::Class,
				},
				pst_data
			));
		}
		case pst::StmtKind::Alias: {
			auto alias = stmt.dynamicCast<pst::Alias>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name     = alias->getName(),
					.kind     = SymbolKind::Alias,
					.is_alias = true,
				},
				pst_data
			));
		}
		case pst::StmtKind::Using: {
			auto using_stmt = stmt.dynamicCast<pst::Using>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name
					= base::StrID(base::strConcat(
									  "<USING> ",
									  using_stmt->getPointed().unlock(ctx)->getNames().front().value
					)
			                          .c_str()),
					.kind        = SymbolKind::Using,
					.is_wildcard = true,
					.is_alias    = true,
				},
				pst_data
			));
		}
		case pst::StmtKind::Variable: {
			auto variable = stmt.dynamicCast<pst::Variable>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name        = variable->getName(),
					.kind        = SymbolKind::Variable,
					.is_wildcard = false,
					.is_alias    = false,
				},
				pst_data
			));
		}
		case pst::StmtKind::Import: {
			// For now only non-wildcard import exist
			auto import = stmt.dynamicCast<pst::Import>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name        = import->getAlias(),
					.kind        = SymbolKind::Import,
					.is_wildcard = false,
					.is_alias    = false,
				},
				pst_data
			));
		}
		case pst::StmtKind::Method: {
			auto method = stmt.dynamicCast<pst::Method>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name = method->getName(),
					.kind = SymbolKind::Method,
				},
				pst_data
			));
		}
		case pst::StmtKind::Field: {
			auto field = stmt.dynamicCast<pst::Field>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name      = field->getName(),
					.kind      = SymbolKind::Field,
					.dependent = true,
				},
				pst_data
			));
		}
		case pst::StmtKind::Constructor: {
			auto constructor = stmt.dynamicCast<pst::Constructor>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name = constructor->getName(),
					.kind = SymbolKind::Constructor,
				},
				pst_data
			));
		}
		case pst::StmtKind::CopyConstructor: {
			auto constructor = stmt.dynamicCast<pst::CopyConstructor>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name = constructor->getName(),
					.kind = SymbolKind::Constructor,
				},
				pst_data
			));
		}
		case pst::StmtKind::Destructor: {
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name = base::StrID("destroy"),
					.kind = SymbolKind::Destructor,
				},
				pst_data
			));
		}
		default:
			break;
		}
		auto stmt_ptr = &*stmt;
		CORE_PANIC(base::strConcat(
			"makeSymbolFromStatement bad symbol kind, stmt: ", typeid(*stmt_ptr).name()
		));
	}

	/**
	 * @brief SymbolData Factory,
	 * Make symbols from PST elements other then statements.
	 * @todo in the future this function should not use dynamic_casts,
	 * and should be merged with makeSymbolFromStatement.
	 */
	CRef<SymbolData> makeSymbolFromPSTElement(ScopeID scope, pst::Access<pst::LangElement> element) {
		if (auto parameter_opt = element.dynamicCast<pst::FunParam>()) {
			auto parameter = parameter_opt.value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name = parameter->getName(),
					.kind = SymbolKind::Parameter,
				},
				{
					.scope       = scope,
					.pst_element = element,
				}
			));
		}
		CORE_PANIC("Not handled PST element in makeSymbolFromPSTElement");
	}

	struct IMPLEMENT_QUERY(QuerySymbolOfSTMT, SymID) {
		/**
		 * @brief Return the scope, that symbol created from given PST element
		 * Should be in.
		 * @note This has to be consistant with QuerySymbolsInScope
		 */
		static ScopeID getPSTElementParentScope(
			query::Context& ctx, pst::AccessLocked<pst::LangElement> element
		) {
			// note: this might become more complicated in the future:
			return ctx.query<QueryPrimaryCodeScopeFor>(element.unlock(ctx)->getParent().value());
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			// Note: we might actually accept nulls in such queries, and just return failed
			// Something to think about as part of #412
			CORE_ASSERT(
				key.element.unlockOpt(ctx),
				"Nullptr element given to QuerySymbolOfSTMT! (add some null handling before "
				"calling it)"
			);
			auto scope = getPSTElementParentScope(ctx, key.element);
			if (auto stmt = key.element.unlock(ctx).dynamicCast<pst::Stmt>())
				return PResult{ makeSymbolFromStatement(ctx, scope, stmt.value()) };
			else
				return PResult{ makeSymbolFromPSTElement(scope, key.element.unlock(ctx)) };
		}

		// @OPT: opt it?
		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolOfSTMT);

	namespace builtin {
		namespace {
			/**
			 * Query all builtin symbols.
			 */
			DECLARE_QUERY(QueryGlobalBuiltinSymbols, query::EmptyKey, CRef<std::vector<SymID>>);

			struct IMPLEMENT_QUERY(QueryGlobalBuiltinSymbols, std::vector<SymID>) {
				static auto provide(Context& ctx, QKey) -> PResult {
					std::vector<SymID> output;

					auto i64_type = tsh::SymbolType<>(
						ctx.query<tsh::QueryIntegralType>(
							{ 64, tsh::IntegralAbstractType::Signedness::Signed }
						),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Mutable
					);
					[[maybe_unused]]
					auto unit_type
						= tsh::SymbolType<>(
							ctx.query<tsh::QueryUnitType>({}),
							tsh::ReferenceKind::Direct,
							tsh::Mutability::Mutable
						);

					std::array<std::pair<base::StrID, tsh::FunctionAbstractType>, 2> function_data
						= {
							  {
								  {
									  base::StrID("builtin_input_i64"),
									  ctx.query<tsh::QueryFunctionType>({ {}, i64_type }),
								  },
								  {
									  base::StrID("builtin_output_i64"),
									  ctx.query<tsh::QueryFunctionType>({ { i64_type }, i64_type }),
								  },
							  },
						  };

					for (auto& [name, type]: function_data) {
						auto sym_data_ref = putInSymtable(
							SymbolData::makeBuiltinFunction(name, BuiltinFunctionData{ type })
						);
						output.push_back(GetSymRef_Functor::make(sym_data_ref));
					}

					return output;
				}

				QUERY_AUTO_CACHE_REF
			};

			QUERY_IMPLEMENTATION_BOILERPLATE(QueryGlobalBuiltinSymbols);
		}

		LookupResult lookupGlobalBuiltins(query::Context& ctx, base::StrID name) {
			LookupResult output{};

			auto builtins = ctx.query<QueryGlobalBuiltinSymbols>({});

			for (auto sym: *builtins)
				if (name == getSymRef(sym)->common.name) output.leaves.push_back(sym);

			return output;
		}
	}

	struct IMPLEMENT_QUERY(QueryLookupInSymbol, LookupResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (key.symbol.ref->common.kind) {
			case SymbolKind::Using:
			case SymbolKind::Namespace:
			case SymbolKind::Import: {
				// @NOTE: for now imports are done via linked scope that looks at root
				// module scope, but in the future it might be changed to custom code

				// @note: this will probably brake for usings,
				// when they look at a symbol without linked scope.
				// We might just delete QueryLinkedScope at some point,
				// when QueryLookupInSymbol will get more and more
				// per-symbol-kind cases.

				auto linked_scope = ctx.query<QueryLinkedScope>(key.symbol);
				return *HInterface::ofScope(linked_scope)
				            .lookup(ctx, key.name, { key.follow_wildcards });
			}

			// @note: here case for variables will be calling TS
			default:
				throw base::NotYetImplemented("Lookup in symbol...");
			}
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInSymbol);

	struct IMPLEMENT_QUERY(QueryLinkedScope, ScopeID) {
		struct QueryLinkedScopeVisitor final: pst::PstVisitorPanicky {
			query::Context& ctx;
			QKey            key;

			QueryLinkedScopeVisitor(query::Context& ctx, QKey key): ctx(ctx), key(key) {}

			base::Optional<ScopeID> result_scope;

			void output(ScopeID out) {
				CORE_ASSERT(result_scope.empty(), "Output already set");
				result_scope.emplace(out);
			}

			void visitUsing(pst::Access<pst::Using> using_stmt) final {
				auto names      = using_stmt->getPointed().unlock(ctx)->getNames();
				auto lookup_res = lookupChain(
					ctx,
					LookupChainKey{ .names       = names,
				                    .begin_scope = scope(key),
				                    .params      = { .with_wildcards = false } }
				);
				CORE_ASSERT(
					lookup_res.hasValue() && not lookup_res.value().empty(),
					"Using points to something that does not exists or is empty"
				);
				auto ret = ctx.query<QueryLinkedScope>({ lookup_res.value().back() });
				output(ret);
			}

			void visitImport(pst::Access<pst::Import> import_stmt) final {
				// @TODO: proper error handling
				auto imported_module = frontend::getRelativeModule(
										   ctx, module(scope(key)), import_stmt->getModulePath()
				)
				                           .value();

				// Here we don't access just root scope, because root scopes are currently empty:
				auto linked_scope = queryRootScopeOfMainModuleFile(ctx, imported_module);

				output(linked_scope);
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (key.ref->common.kind) {
			case SymbolKind::Namespace:
				return queryBodyCodeScopeFor(ctx, key.ref->stmtCast(ctx).value());


			// Special cases for "wildcards":
			case SymbolKind::Using:
			case SymbolKind::Import: {
				QueryLinkedScopeVisitor visitor(ctx, key);
				key.ref->getPSTData()->pst_element.unlock(ctx)->acceptVisitor(visitor);
				return visitor.result_scope.value();
			}
			default:
				throw base::NotYetImplemented("Getting linked scope for some SymbolKind...");
			}
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLinkedScope);

	base::Bit256 KeyOf_LookupInSymbol::queryUnstablePerfectHash() const {
		auto hash_1 = symbol.queryUnstablePerfectHash();
		auto hash_2 = std::hash<base::StrID>()(name);

		return { hash_1, hash_2, static_cast<u64>(follow_wildcards) };
	}

	struct IMPLEMENT_QUERY(QueryDealias, QueryDealias_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			std::vector<tpc::Identifier> pointed_chain;
			if (kind(key) == SymbolKind::Using) {
				auto using_stmt = getSymRef(key)
				                      ->getPSTData()
				                      ->pst_element.unlock(ctx)
				                      .dynamicCast<pst::Using>()
				                      .value();
				pointed_chain = using_stmt->getPointed().unlock(ctx)->getNames();
			} else if (kind(key) == SymbolKind::Alias) {
				auto alias_stmt = getSymRef(key)
				                      ->getPSTData()
				                      ->pst_element.unlock(ctx)
				                      .dynamicCast<pst::Alias>()
				                      .value();
				pointed_chain = alias_stmt->getPointed().unlock(ctx)->getNames();
			} else {
				return SymbolList{ { key } };
			}

			UNPACK_RESULT_MOVE(
				auto lookup_chain =,
				lookupChain(
					ctx, LookupChainKey{ pointed_chain, scope(key), { .with_wildcards = false } }
				)
			);

			return lookup_chain;
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDealias);

	struct IMPLEMENT_QUERY(QueryConstValueOf, query::detail::errors::QResult<i64 COMMA errors::Failed>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Const, "SymID is not a Const");

			const auto const_symbol
				= getSymRef(key)->getPSTData()->pst_element.unlock(ctx).dynamicCast<pst::Const>().value(
				);

			// @TODO: Handle potential lack of value
			return ctx.query<EvalExprToI64>(const_symbol->getValue().value().unlock(ctx)->getExpr());
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryConstValueOf);

}

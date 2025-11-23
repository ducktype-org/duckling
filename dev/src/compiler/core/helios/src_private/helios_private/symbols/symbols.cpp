#include "symbols.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/queries.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup_chain.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <string_id/string_id.hpp>
#include <typesystem/higher/abstract_type.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <base/collections/optional.hpp>
#include <base/collections/stable_container.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/query_impl.hpp>

#include <functional>
#include <unordered_set>
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

	bool isGlobalVar(query::Context& ctx, SymID id) {
		CORE_ASSERT(getSymRef(id)->common.kind == SymbolKind::Variable, "Not a variable.");

		std::function<bool(const pst::Access<pst::LangElement>&)> global_variable_pst_context
			= [&](const pst::Access<pst::LangElement>& el) -> bool {
			switch (el->getElementKind()) {
			case pst::ElementKind::TopLevel:
			case pst::ElementKind::Namespace:
				return true;

			case pst::ElementKind::Class:
			case pst::ElementKind::Fun:
			case pst::ElementKind::If:
			case pst::ElementKind::While:
			case pst::ElementKind::For:
				return false;

			case pst::ElementKind::CodeBlock:
			case pst::ElementKind::CodeBlockOrStmt:
			case pst::ElementKind::Variable:
				// we panic if there is no parent:
				return global_variable_pst_context(el->getParent().value().unlock(ctx));

			default:
				CORE_PANIC("Unexpected pst path of variable");
			}
		};

		return global_variable_pst_context(getSymRef(id)->getPSTData()->pst_element.unlock(ctx));
	}

	SymbolKind kind(SymID id) { return getSymRef(id)->common.kind; }

	ScopeID scope(SymID id) { return getSymRef(id)->getPSTData()->scope; }

	base::Optional<ScopeID> maybeScope(SymID id) {
		variant_match(getSymRef(id)->other) {
			variant_case(PstSymbolData, pst_data) { return pst_data.scope; }
			variant_case_novalue(builtin::BuiltinFunctionData) { return base::Optional<ScopeID>{}; }
			variant_case_novalue(houtgen::GeneratedSymbolData) { return base::Optional<ScopeID>{}; }
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

	std::string prettyDebugPrint(SymID sym, query::Context& ctx) {
		// Short summary
		// 1. Get the symbol's PST element
		// 2. If the element is a statement get its name
		// 3. Get the parent of the pst element
		// 4. Repeat until we reach the root element
		// 5. Concatenate all names with " -> "
		// 6. Prepend the module name

		std::string                                         out = "";
		base::Optional<pst::AccessLocked<pst::LangElement>> pst = symbolPst(sym);
		do {
			if (!pst.value().unlock(ctx)->getParent()) break;
			auto stmt = pst->unlock(ctx).dynamicCast<pst::Stmt>();
			if (!stmt) continue;

			auto name = stmt.value()->getDeclSymbolName();
			if (!name.has_value()) continue;

			if (!out.empty())
				out = base::strConcat(name.value().str(), " -> ", out);
			else
				out = name.value().str();

			// Get the parent of the current pst element
		} while ((pst = pst.value().unlock(ctx)->getParent()));
		auto module_name = compiler::frontend::moduleName(module(scope(sym)));
		out              = base::strConcat(module_name.str(), " -> ", out);

		return out;
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

		for (auto& symbol: symbol_table) output.push_back(GetSymRef_Functor::make(&symbol));
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
	CRef<SymbolData> makeSymbolFromStatement(ScopeID scope, pst::Access<pst::Stmt> stmt) {
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
		case pst::StmtKind::FunDecl: {
			auto function = stmt.dynamicCast<pst::FunDecl>().value();
			return putInSymtable(SymbolData::makePSTSymbolData(
				{
					.name = function->getName(),
					.kind = SymbolKind::FunctionDeclaration,
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
					.name        = using_stmt->getDeclSymbolName().value(),
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
		if (auto parameter_opt = element.dynamicCast<pst::Param>()) {
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
		 * @note This has to be consistent with QuerySymbolsInScope
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
				return PResult{ makeSymbolFromStatement(scope, stmt.value()) };
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

				QUERY_AUTO_CACHE_CREF
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

		QUERY_AUTO_CACHE_CREF
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

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDealias);

	struct IMPLEMENT_QUERY(QueryConstValueOf, query::QResult<ctv::CompileTimeValue COMMA errors::Failed>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Const, "SymID is not a Const");

			const auto const_symbol
				= getSymRef(key)->getPSTData()->pst_element.unlock(ctx).dynamicCast<pst::Const>().value(
				);

			auto ctv = ctx.query<QueryEvaluatePSTExpression>(
				const_symbol->getValue().value().unlock(ctx)->getExpr()
			);
			if (ctv.hasError()) return query::QError(errors::Failed());
			return ctv.value();
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryConstValueOf);

	struct IMPLEMENT_QUERY(QuerySpecifiersOfSymbol, QuerySpecifiersOfSymbol_Result) {
		// @TODO: #1321 do not unlock whole elements, checking the type of the parent would be enough

		/**
		 * @brief Check if the ancestors of PST element `el` match the provided kinds in order,
		 * and return the ancestor if they do.
		 */
		template<typename... Kinds>
		static base::Optional<pst::Access<pst::LangElement>> getAncestor(
			query::Context& ctx, pst::Access<pst::LangElement> el, Kinds... kinds
		) {
			return getAncestorImpl(ctx, el, kinds...);
		}

		// Base case: no more kinds to check → success
		static base::Optional<pst::Access<pst::LangElement>> getAncestorImpl(
			query::Context&, pst::Access<pst::LangElement> el
		) {
			return el;
		}

		// Recursive case: check current kind, then move up
		template<typename... Rest>
		static base::Optional<pst::Access<pst::LangElement>> getAncestorImpl(
			query::Context&               ctx,
			pst::Access<pst::LangElement> el,
			pst::ElementKind              expected,
			Rest... rest
		) {
			if (auto parent = el->getParent()) {
				auto parent_el = parent.value().unlock(ctx);
				if (parent_el->getElementKind() == expected)
					return getAncestorImpl(ctx, parent_el, rest...);
			}
			return {};
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			std::vector<pst::AccessLocked<pst::StmtSpecifier>> specifiers;

			if (kind(key) == SymbolKind::BuiltinFunction) {
				// Builtin functions have no specifiers
				return {};
			}

			if (std::holds_alternative<houtgen::GeneratedSymbolData>(getSymRef(key)->other)) {
				// Generated symbols have no specifiers (for now)
				return {};
			}

			auto pst_element = getSymRef(key)->getPSTData()->pst_element.unlock(ctx);

			// StmtSpecifier only has a "CodeBlockOrStmt" child, which can have a "CodeBlock" child
			// or "Stmt" child.
			//
			// So single statement can have a specifier when it is wrapped in
			// "CodeBlockOrStmt" and "StmtSpecifier" or in the "CodeBlock", "CodeBlockOrStmt" and
			// "StmtSpecifier".
			while (true) {
				if (auto result_stmt = getAncestor(
						ctx,
						pst_element,
						pst::ElementKind::CodeBlockOrStmt,
						pst::ElementKind::StmtSpecifier
					)) {
					pst_element = *std::move(result_stmt);
				} else if (auto result_block = getAncestor(
							   ctx,
							   pst_element,
							   pst::ElementKind::CodeBlock,
							   pst::ElementKind::CodeBlockOrStmt,
							   pst::ElementKind::StmtSpecifier
						   )) {
					pst_element = *std::move(result_block);
				} else {
					break;
				}
				specifiers.emplace_back(pst_element.dynamicCast<pst::StmtSpecifier>().value());
			}

			return specifiers;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySpecifiersOfSymbol);

	namespace houtgen {
		base::Bit256 KeyFor_QueryGeneratedSymbol::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				std::hash<base::StrID>()(name), generated_symbol_data.queryUnstablePerfectHash()
			);
		}

		struct IMPLEMENT_QUERY(QueryGeneratedSymbol, SymID) {
			static auto provide(Context&, QKey key) -> PResult {
				return GetSymRef_Functor::make(putInSymtable(
					SymbolData::makeGeneratedSymbol(key.name, key.generated_symbol_data)
				));
			}

			QUERY_AUTO_CACHE_COPY
		};

		QUERY_IMPLEMENTATION_BOILERPLATE(QueryGeneratedSymbol);
	}

	struct IMPLEMENT_QUERY(QueryDirectFunctionCalls, std::vector<SymID>) {
		struct HoutFunctionCallCollector final:
			  public code::HoutStmtVisitorEmpty,
			  public code::HoutExprVisitorEmpty {
		public:
			std::unordered_set<SymID> called_functions;

			void visitReturnStmt(const code::ReturnStmt& stmt) override {
				stmt.value->acceptVisitor(*this);
			}

			void visitExprStmt(const code::ExprStmt& stmt) override {
				stmt.expr->acceptVisitor(*this);
			}

			void visitIfStmt(const code::IfStmt& stmt) override {
				stmt.condition->acceptVisitor(*this);

				for (const auto& sub_stmt: stmt.then_body.statements)
					sub_stmt->acceptVisitor(*this);
				for (const auto& sub_stmt: stmt.else_body.statements)
					sub_stmt->acceptVisitor(*this);
			}

			void visitWhileStmt(const code::WhileStmt& stmt) override {
				stmt.condition->acceptVisitor(*this);
				for (const auto& sub_stmt: stmt.body.statements) sub_stmt->acceptVisitor(*this);
			}

			void visitVariableStmt(const code::VariableStmt& stmt) override {
				if (stmt.initial_value) stmt.initial_value.value()->acceptVisitor(*this);
			}

			void visitAssignmentStmt(const code::AssignmentStmt& stmt) override {
				stmt.location_expr->acceptVisitor(*this);
				stmt.new_value_expr->acceptVisitor(*this);
			}

			void visitCallExpr(const code::CallExpr& expr) override {
				if (const auto* callee_ident
				    = dynamic_cast<const code::IdentifierExpr*>(expr.callee.get())) {
					if (callee_ident->expression_type.getType().getKind() == tsh::Kind::Function)
						called_functions.insert(callee_ident->symbol);
				}

				expr.callee->acceptVisitor(*this);
				for (const auto& arg: expr.arguments) arg->acceptVisitor(*this);
			}

			void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) override {
				expr.lhs->acceptVisitor(*this);
				expr.rhs->acceptVisitor(*this);
			}

			void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) override {
				expr.expr->acceptVisitor(*this);
			}

			void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& expr) override {
				expr.condition->acceptVisitor(*this);
				expr.if_true->acceptVisitor(*this);
				expr.if_false->acceptVisitor(*this);
			}

			void visitParenthesisExpr(const code::ParenthesisExpr& expr) override {
				expr.inner->acceptVisitor(*this);
			}

			void visitSequenceExpr(const code::SequenceExpr& expr) override {
				for (const auto& sub_expr: expr.expressions) sub_expr->acceptVisitor(*this);
			}

			void visitAccessExpr(const code::AccessExpr& expr) override {
				expr.base->acceptVisitor(*this);
			}

			void visitChainComparisonExpr(const code::ChainComparisonExpr& expr) override {
				for (const auto& sub_expr: expr.expressions) sub_expr->acceptVisitor(*this);
			}

			void visitTupleTypeConstructorExpr(const code::TupleTypeConstructorExpr& expr) override {
				for (const auto& sub_expr: expr.elements) sub_expr->acceptVisitor(*this);
			}

			void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& expr
			) override {
				for (const auto& sub_expr: expr.subtypes) sub_expr->acceptVisitor(*this);
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Function,
				"Query function dependencies called on non-function symbol"
			);

			auto        fun_hout_result = ctx.query<QueryCodeOfFun>(key);
			const auto& function_body   = fun_hout_result.body;

			HoutFunctionCallCollector visitor;
			for (const auto& stmt: function_body->statements) stmt->acceptVisitor(visitor);
			return std::ranges::to<std::vector<SymID>>(visitor.called_functions);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDirectFunctionCalls);

	struct IMPLEMENT_QUERY(QueryTransitiveFunctionCalls, std::vector<SymID>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Function,
				"Query transitive function dependencies called on non-function symbol"
			);

			std::vector<SymID>        worklist;
			std::unordered_set<SymID> visited_functions;
			std::vector<SymID>        all_dependencies;

			worklist.push_back(key);  // Insert root function SymID.
			visited_functions.insert(key);

			while (!worklist.empty()) {
				SymID current_func = worklist.back();
				worklist.pop_back();

				all_dependencies.push_back(current_func);

				auto direct_dependencies = ctx.query<QueryDirectFunctionCalls>(current_func);

				for (const SymID& dependency: *direct_dependencies) {
					if (!visited_functions.contains(dependency)) {
						visited_functions.insert(dependency);
						worklist.push_back(dependency);
					}
				}
			}
			return all_dependencies;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTransitiveFunctionCalls);
}

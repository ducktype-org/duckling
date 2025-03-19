#include "symbols.hpp"

#include <vector>

#include <base/exceptions.hpp>
#include <base/string_id.hpp>
#include <base/stable_hashmap.hpp>
#include <base/stable_container.hpp>
#include <base/variant.hpp>
#include <base/optional.hpp>

#include <query_framework/query_impl.hpp>
#include <pst_parser/elements/elements.hpp>
#include <pst_parser/pst_visitor.hpp>
#include <typesystem/higher/abstract_type.hpp>

#include <pst_parser/elements/hierarchy/not_statements.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <helios/comp_time/type_eval.hpp>
#include <helios/comp_time/int_eval.hpp>

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
	struct SymbolData final {
		// created when creating SymbolData:
		ScopeID     scope;
		base::StrID name;
		bool        anonymous   = false;
		bool        is_wildcard = false;
		bool        is_alias    = false;
		bool        dependent   = false;
		SymbolKind  kind;

		pst::AccessLocked<pst::LangElement> pst_element;

		/**
		 * Return associated pst_element cast to Stmt.
		 * Panics if element is not a statement.
		 */
		[[nodiscard]]
		base::Optional<pst::Access<pst::Stmt>> stmtCast(query::detail::ContextType& ctx) const {
			return pst_element.unlock(ctx).dynamicCast<pst::Stmt>();
		}
	};

	/**
	 * @brief Helper struct used to access private SymID data.
	 */
	struct GetSymRef_Functor final {
		static auto get(SymID id) { return id.ref; }
	};

	auto getSymRef(SymID id) { return GetSymRef_Functor::get(id); }

	bool isWildcard(SymID id) { return getSymRef(id)->is_wildcard; }

	base::StrID name(SymID id) { return getSymRef(id)->name; }

	SymbolKind kind(SymID id) { return getSymRef(id)->kind; }

	ScopeID scope(SymID id) { return getSymRef(id)->scope; }

	base::Optional<pst::Access<pst::Stmt>> stmt(query::Context& ctx, SymID id) {
		return getSymRef(id)->stmtCast(ctx);
	}

	pst::AccessLocked<pst::LangElement> symbolPst(SymID id) { return getSymRef(id)->pst_element; }

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
	Ref<SymbolData> makeSymbolFromStatement(ScopeID scope, pst::Access<pst::Stmt> stmt) {
		// @TODO: change this function to visitor to avoid dynamic_casts

		switch (stmt->getStmtKind()) {
		case pst::StmtKind::Fun: {
			auto function = stmt.dynamicCast<pst::Fun>().value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = function->getName(),
				.kind        = SymbolKind::Function,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Namespace: {
			auto namespace_stmt = stmt.dynamicCast<pst::Namespace>().value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = namespace_stmt->getName(),
				.kind        = SymbolKind::Namespace,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Const: {
			auto const_stmt = stmt.dynamicCast<pst::Const>().value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = const_stmt->getName(),
				.kind        = SymbolKind::Const,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Class: {
			auto class_stmt = stmt.dynamicCast<pst::Class>().value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = class_stmt->getName(),
				.kind        = SymbolKind::Class,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Alias: {
			auto alias = stmt.dynamicCast<pst::Alias>().value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = alias->getName(),
				.is_alias    = true,
				.kind        = SymbolKind::Alias,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Using: {
			auto using_stmt = stmt.dynamicCast<pst::Using>().value();
			return putInSymtable(SymbolData{
				.scope = scope,
				.name
				= base::StrID(base::strConcat("<USING> ", using_stmt->getPointed().front()).c_str()
			    ),
				.is_wildcard = true,
				.is_alias    = true,
				.kind        = SymbolKind::Using,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Variable: {
			auto variable = stmt.dynamicCast<pst::Variable>().value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = variable->getName(),
				.is_wildcard = false,
				.is_alias    = false,
				.kind        = SymbolKind::Variable,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Import: {
			// For now only non-wildcard import exist
			auto import = stmt.dynamicCast<pst::Import>().value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = import->getAlias(),
				.is_wildcard = false,
				.is_alias    = false,
				.kind        = SymbolKind::Import,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Method: {
			auto method = stmt.dynamicCast<pst::Method>().value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = method->getName(),
				.kind        = SymbolKind::Method,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Field: {
			auto field = stmt.dynamicCast<pst::Field>().value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = field->getName(),
				.kind        = SymbolKind::Field,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Constructor: {
			auto constructor = stmt.dynamicCast<pst::Constructor>().value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = constructor->getName(),
				.kind        = SymbolKind::Constructor,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Destructor: {
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = base::StrID("destroy"),
				.kind        = SymbolKind::Destructor,
				.pst_element = stmt,
			});
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
	Ref<SymbolData> makeSymbolFromPSTElement(ScopeID scope, pst::Access<pst::LangElement> element) {
		if (auto parameter_opt = element.dynamicCast<pst::FunParam>()) {
			auto parameter = parameter_opt.value();
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = parameter->getName(),
				.kind        = SymbolKind::Parameter,
				.pst_element = element,
			});
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
			return ctx.query<QueryPrimaryCodeScopeFor>(element.unlock(ctx)->getParent());
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

	struct IMPLEMENT_QUERY(QueryLookupInSymbol, LookupResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (key.symbol.ref->kind) {
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
				return *ctx.query<QueryLookupInScope>(
					{ linked_scope, key.name, key.follow_wildcards }
				);
			}

			// @note: here case for variables will be calling TS
			default:
				throw base::NotYetImplemented("Lookup in symbol...");
			}
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInSymbol);

	struct LookupChainKey final {
		std::vector<base::StrID> names;
		ScopeID                  begin_scope;
		bool                     follow_wildcards;
	};

	/**
	 * @brief Query extension for looking-up chain of names
	 */
	errors::HResult<SymbolList, errors::Ambiguity, errors::SymbolNotFound>
		lookupChain(query::Context& ctx, const LookupChainKey& key) {
		CORE_ASSERT(!key.names.empty(), "lookupDotted received zero names");

		// initial symbol:
		auto first = ctx.query<QueryLookupInScopeAndParents>(
			{ key.begin_scope, key.names[0], key.follow_wildcards }
		);

		UNPACK_RESULT(SymbolList result =, first->getAsSingle());

		if (key.names.size() == 1) return result;

		for (usize i = 1; i < key.names.size(); i++) {
			auto append_res = ctx.query<QueryLookupInSymbol>({
				result.back(),
				key.names[i],
				key.follow_wildcards,
			});

			UNPACK_RESULT(auto single_append_res =, append_res->getAsSingle());
			result.insert(result.end(), single_append_res.begin(), single_append_res.end());
		}
		return result;
	}

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
				auto names      = using_stmt->getPointed();
				auto lookup_res = lookupChain(ctx, LookupChainKey{ names, scope(key), false });
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
			switch (key.ref->kind) {
			case SymbolKind::Namespace:
				return queryBodyCodeScopeFor(ctx, key.ref->stmtCast(ctx).value());


			// Special cases for "wildcards":
			case SymbolKind::Using:
			case SymbolKind::Import: {
				QueryLinkedScopeVisitor visitor(ctx, key);
				key.ref->pst_element.unlock(ctx)->acceptVisitor(visitor);
				return visitor.result_scope.value();
			}
			default:
				throw base::NotYetImplemented("Getting linked scope for some SymbolKind...");
			}
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLinkedScope);

	base::HashT KeyOf_LookupInSymbol::customPerfectHash() const {
		auto hash_1 = base::perfectHash(symbol);
		auto hash_2 = std::hash<base::StrID>()(name);

		// @FIXME: this does not work:
		return (hash_1 * 143 + hash_2 * 7) * 2 + follow_wildcards;
	}

	struct IMPLEMENT_QUERY(QueryDealias, QueryDealias_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// @TODO: this does not handle usings.
			if (kind(key) == SymbolKind::Using)
				std::cerr << "Warning: QueryDealias does not handle usings (@TODO).\n";

			if (kind(key) != SymbolKind::Alias) return SymbolList{ key };

			auto alias_definition
				= getSymRef(key)->pst_element.unlock(ctx).dynamicCast<pst::Alias>().value();

			bool       first_symbol = true;
			SymbolList result;
			for (auto pointed: alias_definition->getPointed()) {
				auto pointed_symbol_lookup
					= first_symbol
				        ? ctx.query<QueryLookupInScopeAndParents>({ scope(key), pointed, false })
				        : ctx.query<QueryLookupInSymbol>({ result.back(), pointed, false });
				auto path = pointed_symbol_lookup->getAsSingle();
				if (path.hasError()) {
					variant_match(path.error()) {
						variant_case(errors::Ambiguity, _) {
							// this error might need to be reported earlier:
							ctx.log(
								dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>::
									make(
										alias_definition->getSourcePosition(),
										"Ambiguity in dealias"
									)
							);

							return errors::HError(errors::Failed());
						}
						variant_case(errors::SymbolNotFound, _) {
							// this error might need to be reported earlier:
							ctx.log(
								dia::PlaceholderMessage<dia::Error, dia::Message::Domain::Lookup>::
									make(
										alias_definition->getSourcePosition(),
										"Symbol not found in dealias"
									)
							);

							return errors::HError(errors::Failed());
						}
					}
					CORE_PANIC("Invalid state");
				}
				for (auto path_symbol: path.value()) {
					UNPACK_RESULT(const auto& dealiased =, *ctx.query<QueryDealias>(path_symbol));
					result.insert(result.end(), dealiased.begin(), dealiased.end());
				}

				first_symbol = false;
			}

			return result;
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDealias);

	struct IMPLEMENT_QUERY(QueryConstValueOf, errors::HResult<i64 COMMA errors::Failed>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Const, "SymID is not a Const");

			const auto const_symbol
				= getSymRef(key)->pst_element.unlock(ctx).dynamicCast<pst::Const>().value();

			return ctx.query<EvalExprToI64>(const_symbol->getValue().unlock(ctx)->getExpr());
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryConstValueOf);

	struct IMPLEMENT_QUERY(QueryTypeOfSymbol, QuerySymbolType_Result) {
		class PstVisitor_GetTypeOf final: public pst::PstVisitorPanicky {
			Context& ctx;

			// @TODO: make failure more explicit

			void setTypeOfSymbol(const tsh::SymbolType<>& type) {
				if (symbol_type.has_value())
					CORE_PANIC("Attempted to set type of symbol in visitor a second time.");
				symbol_type = type;
			}

			void setTypeOfSymbolByAbstractType(const tsh::AbstractType& type) {
				if (symbol_type.has_value())
					CORE_PANIC("Attempted to set type of symbol in visitor a second time.");
				symbol_type = tsh::SymbolType{
					type,
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}

			void setTypeOfSymbol(pst::Access<pst::ExprElement> expr) {
				auto tp = ctx.query<EvalExprToType>(pst::AccessLocked<pst::ExprElement>(expr));
				if (tp.hasValue()) setTypeOfSymbol(tp.value());
			}

		public:
			PstVisitor_GetTypeOf(Context& ctx): ctx(ctx) {}

			base::Optional<tsh::SymbolType<>> symbol_type;

			void visitConst(pst::Access<pst::Const> stmt) final {
				setTypeOfSymbol(stmt->getType().unlock(ctx)->getExpr().unlock(ctx));
			}

			void visitVariable(pst::Access<pst::Variable> stmt) final {
				setTypeOfSymbol(stmt->getType().unlock(ctx)->getExpr().unlock(ctx));
			}

			void visitField(pst::Access<pst::Field> field) final {
				setTypeOfSymbol(field->getType().unlock(ctx)->getExpr().unlock(ctx));
			}

			void visitFun(pst::Access<pst::Fun> fun) final {
				auto locked_params = fun->getParams();
				auto params        = locked_params.unlock(ctx);
				auto ret           = fun->getRet();

				std::vector<tsh::SymbolType<>> param_types{};
				param_types.reserve(params->size());

				for (auto param: *params) {
					auto param_symbol = ctx.query<QuerySymbolOfSTMT>({ param });
					auto param_type   = ctx.query<QueryTypeOfSymbol>({ param_symbol });

					if (param_type->hasValue()) {
						param_types.emplace_back(param_type->value());
					} else {
						// we just fail here, because we can't continue without type
						return;
					}
				}

				// Default return type is a direct unit.
				tsh::SymbolType<> ret_type = tsh::SymbolType<>{
					ctx.query<tsh::QueryUnitType>({}),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};

				if (ret.has_value()) {
					auto parsed = ctx.query<EvalExprToType>(ret.value().unlock(ctx)->getExpr());
					if (parsed.hasValue()) {
						ret_type = parsed.value();
					} else {
						// we just fail here, because we can't continue without type
						return;
					}
				}
				setTypeOfSymbolByAbstractType(ctx.query<tsh::QueryFunctionType>({ param_types,
				                                                                  ret_type }));
			}

			void visitClass(pst::Access<pst::Class>) final {
				setTypeOfSymbolByAbstractType(ctx.query<tsh::QueryMetaType>({}));
			}

			void visitNamespace(pst::Access<pst::Namespace>) final {
				setTypeOfSymbolByAbstractType(ctx.query<tsh::QueryNamespaceType>({}));
			}

			void visitImport(pst::Access<pst::Import>) final {
				setTypeOfSymbolByAbstractType(ctx.query<tsh::QueryImportType>({}));
			}

			void visitFunParam(pst::Access<pst::FunParam> param) final {
				setTypeOfSymbol(param->getType().unlock(ctx)->getExpr().unlock(ctx));
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto symbol_ref = getSymRef(key);

			PstVisitor_GetTypeOf visitor(ctx);
			symbol_ref->pst_element.unlock(ctx)->acceptVisitor(visitor);

			if_opt_some(visitor.symbol_type, type) return type;
			return errors::HError(errors::Failed());
		}

		QUERY_AUTO_CACHE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeOfSymbol);

	struct IMPLEMENT_QUERY(QueryTypeFromDefinition, QuerySymbolType_Result) {
		class PstVisitor_GetTypeFromDefinition final: public pst::PstVisitorPanicky {
			Context&    ctx;
			const QKey& key;

			void setTypeOfDefinition(const tsh::SymbolType<>& type) {
				if (definition_symbol_type.has_value())
					CORE_PANIC("Attempted to set type of definition in visitor a second time.");
				definition_symbol_type = type;
			}

		public:
			PstVisitor_GetTypeFromDefinition(Context& ctx, const QKey& key): ctx(ctx), key(key) {}

			base::Optional<tsh::SymbolType<>> definition_symbol_type;

			void visitClass(pst::Access<pst::Class>) final {
				definition_symbol_type = tsh::SymbolType<>{
					ctx.query<tsh::QueryClassType>(key),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto symbol_ref = getSymRef(key);

			PstVisitor_GetTypeFromDefinition visitor(ctx, key);
			symbol_ref->pst_element.unlock(ctx)->acceptVisitor(visitor);
			return visitor.definition_symbol_type.value();
		}

		QUERY_AUTO_CACHE_REF;
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeFromDefinition)

	struct IMPLEMENT_QUERY(QueryClassSymbolData, QueryClassSymbolData_Result) {
		struct ClassDataParser final: pst::PstVisitorPanicky {
			query::Context& ctx;

			ClassDataParser(query::Context& ctx): ctx(ctx) {}

			base::Optional<base::StrID>                            name;
			base::Optional<pst::AccessLocked<pst::ExprElement>>    base_class;
			base::Optional<pst::AccessLocked<pst::ImplementsList>> implements;

			void visitClass(pst::Access<pst::Class> stmt) final {
				name = stmt->getName();
				if (auto base = stmt->getBase().unlockOpt(ctx)) base_class = base.value();
				if (auto implements = stmt->getImplements().unlockOpt(ctx))
					this->implements = implements.value();
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Class, "Symbol is not a class");

			auto class_stmt = getSymRef(key)->stmtCast(ctx).value();

			auto class_body_scope = queryBodyCodeScopeFor(ctx, class_stmt);
			auto class_symbols    = ctx.query<QuerySymbolsInScope>(class_body_scope);

			ClassSymbolData class_info;
			for (auto sym: *class_symbols) {
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
			auto class_data_parser = ClassDataParser(ctx);
			class_stmt->acceptVisitor(class_data_parser);
			class_info.name = class_data_parser.name.value();

			if_opt_some(class_data_parser.base_class, base) {
				auto tp = ctx.query<EvalExprToType>({ base });
				if (tp.hasValue()) {
					// @TODO: Raise errors, here, or preferably earlier, if the symbol type of
					// the base class is given with any specifiers apart from the abstract type.
					class_info.base = tp.value().getType();
				} else {
					// We just fail here, error should be reported by EvalExprToType
					return errors::HError(errors::Failed());
				}
			}

			if_opt_some(class_data_parser.implements, implements) {
				for (auto&& interface: *implements.unlock(ctx)) {
					auto tp = ctx.query<EvalExprToType>(interface.unlock(ctx)->getExpr());
					if (tp.hasValue()) {
						// @TODO: Raise errors, here, or preferably earlier, if the symbol type of
						// the interface is given with any specifiers apart from the abstract type.
						class_info.implements.push_back(tp.value().getType());
					} else {
						// We just fail here, error should be reported by EvalExprToType
						return errors::HError(errors::Failed());
					}
				}
			}

			return class_info;
		}

		QUERY_AUTO_CACHE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassSymbolData);
}

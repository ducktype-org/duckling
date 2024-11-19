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

#include "../lookup_result.hpp"
#include "../scope_symbol_id.hpp"
#include "../scopes/scopes.hpp"
#include "../pst_ref.hpp"
#include "../helios_errors.hpp"
#include "../helios_result.hpp"
#include "pst_parser/elements/hierarchy/not_statements.hpp"
#include "typesystem/higher/queries/types.hpp"
#include <base/optional.hpp>
#include <typesystem/higher/type_info.hpp>

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

		// we will need to cast it:
		PstRef<pst::Stmt> pst_stmt;
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
				= base::StrID(base::strConcat("<USING> ", using_->getPointed().front()).c_str()),
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
				.name     = base::StrID("destroy"),
				.kind     = SymbolKind::Destructor,
				.pst_stmt = stmt,
			});
		}
		default:
			break;
		}
		auto stmt_ptr = stmt.get();
		CORE_PANIC(base::strConcat(
			"makeSymbolFromStatement bad symbol kind, stmt: ", typeid(*stmt_ptr).name()
		));
	}

	struct IMPLEMENT_QUERY(QuerySymbolOfSTMT, SymID) {
		inline static base::HashMap<pst::PstID, ScopeID> parent_map;

		static auto provide(Context&, QKey key) -> PResult {
			auto pst_id = key.stmt->getID();

			// This is a sanity check, that might be rendered obsolete
			// once scope refactor will be introduced.
			// It currently prevents some scope bugs/inconsistencies from happening.
			if (parent_map.contains(pst_id)) {
				CORE_ASSERT(
					parent_map.at(pst_id) == key.scope, "Parent mismatch in QuerySymbolOfSTMT"
				);
			} else {
				parent_map.put(pst_id, key.scope);
			}

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
		LookupResult first = ctx.query<QueryLookupInScopeAndParents>(
			{ key.begin_scope, key.names[0], key.follow_wildcards }
		);

		UNPACK_RESULT(SymbolList result =, first.getAsSingle());

		if (key.names.size() == 1) return result;

		for (usize i = 1; i < key.names.size(); i++) {
			auto append_res = ctx.query<QueryLookupInSymbol>({
				result.back(),
				key.names[i],
				key.follow_wildcards,
			});

			UNPACK_RESULT(auto single_append_res =, append_res.getAsSingle());
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
				CORE_ASSERT(
					lookup_res.hasValue() && not lookup_res.value().empty(),
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

			auto&& alias_definition
				= dynamic_cast<const pst::Alias*>(getSymRef(key)->pst_stmt.get());

			bool       first_symbol = true;
			SymbolList result;
			for (auto&& pointed: alias_definition->getPointed()) {
				auto&& pointed_symbol_lookup
					= first_symbol
				        ? ctx.query<QueryLookupInScopeAndParents>({ scope(key), pointed, false })
				        : ctx.query<QueryLookupInSymbol>({ result.back(), pointed, false });
				auto&& path = pointed_symbol_lookup.getAsSingle();
				if (path.hasError()) {
					variant_match(path.error()) {
						variant_case(errors::Ambiguity, _) {
							// @TODO: Report an error
							return errors::HError(errors::Failed());
						}
						variant_case(errors::SymbolNotFound, _) {
							// @TODO: Report an error
							return errors::HError(errors::Failed());
						}
					}
					CORE_PANIC("Invalid state");
				}
				for (auto&& path_symbol: path.value()) {
					UNPACK_RESULT(auto&& dealiased =, ctx.query<QueryDealias>(path_symbol));
					result.insert(result.end(), dealiased.begin(), dealiased.end());
				}

				first_symbol = false;
			}

			return result;
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDealias);

	struct IMPLEMENT_QUERY(QueryConstValueOf, errors::HResult<i32 COMMA errors::Failed>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Const, "SymID is not a Const");

			const auto const_symbol
				= dynamic_cast<const pst::Const*>(getSymRef(key)->pst_stmt.get());
			const ScopeID key_scope = scope(key);

			// @EXPR: Find all calls to fromPST and change them to use query.
			auto eval = code::Expr::fromPST(ctx, key_scope, const_symbol->getValue());
			if (eval.hasError()) {
				// @TODO: Report an error
				return errors::HError(errors::Failed());
			}
			return eval.value()->evaluateValue(ctx);
		}

		QUERY_AUTO_CACHE_PRESULT_UNSTABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryConstValueOf);

	/**
	 * Parses the expression assuming it evaluates to a type. Panics otherwise.
	 * @param ctx Context passed to a query.
	 * @param expr A PST ExprElement, that has been written in the source code.
	 * @param expr_scope A scope where the expression has been expressed.
	 * @return tsh::TypeInfo with information about the evaluated type.
	 */
	ParseTypeFromExpr_Result
		parseTypeFromExpr(query::Context& ctx, ScopeID expr_scope, PstRef<pst::ExprElement> expr) {
		auto parsed = code::Expr::fromPST(ctx, expr_scope, expr);
		if (parsed.hasError()) return errors::HError(parsed.error());
		auto tree = std::move(parsed.value());
		return tree->type_desc.getType();
	}

	struct IMPLEMENT_QUERY(QueryTypeOfSymbol, QueryType_Result) {
		class PstStmtVisitor_GetTypeOf final: public pst::PstStmtVisitorPanicky {
			Context&    ctx;
			const QKey& key;

			void setTypeOfSymbol(const tsh::TypeInfo& type) {
				if (symbol_type_info.has_value())
					CORE_PANIC("Attempted to set type of symbol in visitor a second time.");
				symbol_type_info = type;
			}

			void setTypeOfSymbol(PstRef<pst::ExprElement> expr) {
				auto tp = parseTypeFromExpr(ctx, scope(key), expr);
				if (tp.hasValue()) setTypeOfSymbol(tp.value());
			}

		public:
			PstStmtVisitor_GetTypeOf(Context& ctx, const QKey& key): ctx(ctx), key(key) {}

			base::Optional<tsh::TypeInfo> symbol_type_info;

			void visitConst(const pst::Const& stmt) override { setTypeOfSymbol(stmt.getType()); }

			void visitVariable(const pst::Variable& stmt) override {
				setTypeOfSymbol(stmt.getType());
			}

			void visitField(const pst::Field& field) override { setTypeOfSymbol(field.getType()); }

			void visitFun(const pst::Fun& fun) override {
				auto params = fun.getParams();
				auto ret    = fun.getRet();

				std::vector<tsh::TypeInfo> param_types{};
				param_types.reserve(params->size());

				for (auto param: params) {
					auto&& parse_type_res = parseTypeFromExpr(ctx, scope(key), param->getType());
					if (parse_type_res.hasValue()) {
						param_types.emplace_back(parse_type_res.value());
					} else {
						// @TODO: Report an error
						return;
					}
				}
				tsh::TypeInfo ret_type = ctx.query<tsh::QueryUnitType>({});
				if (ret.has_value()) {
					auto&& parsed = parseTypeFromExpr(ctx, scope(key), ret.value());
					if (parsed.hasValue()) {
						ret_type = parsed.value();
					} else {
						// @TODO: Report an error
						return;
					}
				}
				setTypeOfSymbol(ctx.query<tsh::QueryFunctionType>({ param_types, ret_type }));
			}

			void visitClass(const pst::Class&) override {
				// This method is empty on purpose, because we still want a panicky visitor
			}

			void visitNamespace(const pst::Namespace&) override {
				setTypeOfSymbol(ctx.query<tsh::QueryNamespaceType>({}));
			}

			void visitImport(const pst::Import&) override {
				setTypeOfSymbol(ctx.query<tsh::QueryNamespaceType>({}));
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto&& symbol_ref = getSymRef(key);

			PstStmtVisitor_GetTypeOf visitor(ctx, key);
			symbol_ref->pst_stmt->acceptVisitor(visitor);
			if_opt_some(visitor.symbol_type_info, type) return type;
			return errors::HError(errors::Failed());
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeOfSymbol);

	struct IMPLEMENT_QUERY(QueryTypeFromDefinition, QueryType_Result) {
		class PstStmtVisitor_GetTypeFromDefinition final: public pst::PstStmtVisitorPanicky {
			Context&    ctx;
			const QKey& key;

			void setTypeOfDefinition(const tsh::TypeInfo& type) {
				if (definition_type_info.has_value())
					CORE_PANIC("Attempted to set type of definition in visitor a second time.");
				definition_type_info = type;
			}

		public:
			PstStmtVisitor_GetTypeFromDefinition(Context& ctx, const QKey& key):
				  ctx(ctx),
				  key(key) {}

			base::Optional<tsh::TypeInfo> definition_type_info;

			void visitClass(const pst::Class&) override {
				definition_type_info = ctx.query<tsh::QueryClassType>(key);
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

	struct IMPLEMENT_QUERY(QueryTypeOfSymbolOrDefinition, QueryType_Result) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto x = ctx.query<QueryTypeOfSymbol>(key);
			if (x.hasValue()) return x.value();

			return ctx.query<QueryTypeFromDefinition>(key);
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF;
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeOfSymbolOrDefinition)

	struct IMPLEMENT_QUERY(QueryClassSymbolData, QueryClassSymbolData_Result) {
		struct ClassDataParser final: pst::PstStmtVisitorPanicky {
			base::Optional<base::StrID>                                name;
			base::Optional<tpc::ParserCBorrowRef<pst::ExprElement>>    base_class;
			base::Optional<tpc::ParserCBorrowRef<pst::ImplementsList>> implements;

			void visitClass(const pst::Class& stmt) override {
				name = stmt.getName();
				if (auto&& base = stmt.getBase(); base != nullptr) base_class = base;
				if (auto&& implements = stmt.getImplements(); implements != nullptr)
					this->implements = implements;
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Class, "Symbol is not a class");

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
				auto tp = parseTypeFromExpr(ctx, class_scope, base);
				if (tp.hasValue()) {
					class_info.base = tp.value();
				} else {
					// @TODO: Report an error
					return errors::HError(errors::Failed());
				}
			}

			if_opt_some(class_data_parser.implements, implements) {
				for (auto&& interface: implements) {
					auto tp = parseTypeFromExpr(ctx, class_scope, interface);
					if (tp.hasValue()) {
						class_info.implements.push_back(tp.value());
					} else {
						// @TODO: Report an error
						return errors::HError(errors::Failed());
					}
				}
			}

			return class_info;
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassSymbolData);

	struct
		IMPLEMENT_QUERY(QueryHOUTExprTreeOfSym, errors::HResult<base::Box<code::Expr> COMMA errors::Failed>) {
		struct PstStmtVisitor_GetHOUTExprTree final: public pst::PstStmtVisitorPanicky {
			Context& ctx;
			ScopeID  scope;

			errors::HResult<base::Box<code::Expr>, errors::Failed> expr_tree;

			void setExprTree(const pst::ParserCBorrowRef<pst::ExprElement>& expr) {
				expr_tree = code::Expr::fromPST(ctx, scope, expr);
			}

		public:
			PstStmtVisitor_GetHOUTExprTree(Context& ctx, ScopeID scope): ctx(ctx), scope(scope) {}

			void visitConst(const pst::Const& stmt) override { setExprTree(stmt.getValue()); }

			void visitVariable(const pst::Variable& stmt) override { setExprTree(stmt.getType()); }
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto&& symbol_ref = getSymRef(key);

			PstStmtVisitor_GetHOUTExprTree visitor(ctx, scope(key));
			symbol_ref->pst_stmt->acceptVisitor(visitor);
			return std::move(visitor.expr_tree);
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHOUTExprTreeOfSym);
}

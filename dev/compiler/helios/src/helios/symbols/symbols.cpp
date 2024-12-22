#include "symbols.hpp"

#include <vector>
#include <cmath>

#include <base/exceptions.hpp>
#include <base/string_id.hpp>
#include <base/stable_hashmap.hpp>
#include <base/stable_container.hpp>
#include <base/variant.hpp>
#include <base/unique_pointer.hpp>
#include <base/optional.hpp>

#include <query_framework/query_impl.hpp>
#include <pst_parser/elements/elements.hpp>
#include <pst_parser/pst_visitor.hpp>
#include <typesystem/higher/type_info.hpp>

#include <pst_parser/elements/hierarchy/not_statements.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <helios/hout/elements.hpp>
#include <helios/hout/visitors.hpp>

#include "../hout/comp_time.hpp"

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

		CRef<pst::LangElement> pst_element;

		/**
		 * Retrun associated pst_element casted to Stmt.
		 * Panics if element is not a statement.
		 */
		[[nodiscard]]
		CRef<pst::Stmt> stmtCast() const {
			return dynamic_cast<const pst::Stmt*>(&*pst_element);
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

	CRef<pst::Stmt> stmt(SymID id) { return getSymRef(id)->stmtCast(); }

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
	Ref<SymbolData> makeSymbolFromStatement(ScopeID scope, CRef<pst::Stmt> stmt) {
		// @TODO: change this function to visitor to avoid dynamic_casts

		switch (stmt->getStmtKind()) {
		case pst::StmtKind::Fun: {
			auto function_ = dynamic_cast<const pst::Fun*>(&*stmt);
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = function_->getName(),
				.kind     = SymbolKind::Function,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Namespace: {
			auto namespace_ = dynamic_cast<const pst::Namespace*>(&*stmt);
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = namespace_->getName(),
				.kind     = SymbolKind::Namespace,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Const: {
			auto const_ = dynamic_cast<const pst::Const*>(&*stmt);
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = const_->getName(),
				.kind     = SymbolKind::Const,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Class: {
			auto class_ = dynamic_cast<const pst::Class*>(&*stmt);
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = class_->getName(),
				.kind     = SymbolKind::Class,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Alias: {
			auto alias_ = dynamic_cast<const pst::Alias*>(&*stmt);
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = alias_->getName(),
				.is_alias = true,
				.kind     = SymbolKind::Alias,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Using: {
			auto using_ = dynamic_cast<const pst::Using*>(&*stmt);
			return putInSymtable(SymbolData{
				.scope = scope,
				.name
				= base::StrID(base::strConcat("<USING> ", using_->getPointed().front()).c_str()),
				.is_wildcard = true,
				.is_alias    = true,
				.kind        = SymbolKind::Using,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Variable: {
			auto variable_ = dynamic_cast<const pst::Variable*>(&*stmt);
			return putInSymtable(SymbolData{
				.scope       = scope,
				.name        = variable_->getName(),
				.is_wildcard = false,
				.is_alias    = false,
				.kind        = SymbolKind::Variable,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Import: {
			// For now only non-wildcard import exist
			auto import = dynamic_cast<const pst::Import*>(&*stmt);
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
			auto method = dynamic_cast<const pst::Method*>(&*stmt);
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = method->getName(),
				.kind     = SymbolKind::Method,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Field: {
			auto field = dynamic_cast<const pst::Field*>(&*stmt);
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = field->getName(),
				.kind     = SymbolKind::Field,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Constructor: {
			auto constructor = dynamic_cast<const pst::Constructor*>(&*stmt);
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = constructor->getName(),
				.kind     = SymbolKind::Constructor,
				.pst_element = stmt,
			});
		}
		case pst::StmtKind::Destructor: {
			return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = base::StrID("destroy"),
				.kind     = SymbolKind::Destructor,
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
	Ref<SymbolData> makeSymbolFromPSTElement(ScopeID scope, CRef<pst::LangElement> element) {
		if (auto parameter = dynamic_cast<const pst::FunParam*>(&*element)) {
				return putInSymtable(SymbolData{
				.scope    = scope,
				.name     = parameter->getName(),
				.kind     = SymbolKind::Parameter,
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
		static ScopeID
			getPSTElementParentScope(query::Context& ctx, MCRef<pst::LangElement> element) {
			// note: this might become more complicated in the future:
			return ctx.query<QueryPrimaryCodeScopeFor>({ element->getParent().value() });
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				key.element.toOpt().has_value(),
				"Nullptr element given to QuerySymbolOfSTMT! (add some null handling before calling it)"
			);
			auto scope = getPSTElementParentScope(ctx, key.element);
			if (auto stmt = dynamic_cast<const pst::Stmt*>(&*key.element)) {
				return PResult{ makeSymbolFromStatement(scope, stmt) };
			}
			else {
				return PResult{ makeSymbolFromPSTElement(scope, key.element.toOpt().value()) };
			}
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
		struct QueryLinkedScopeVisitor: pst::PstStmtVisitorPanicky {
			query::Context& ctx;
			QKey            key;

			QueryLinkedScopeVisitor(query::Context& ctx, QKey key): ctx(ctx), key(key) {}

			base::Optional<ScopeID> result_scope;

			void output(ScopeID out) {
				CORE_ASSERT(result_scope.empty(), "Output already set");
				result_scope.emplace(out);
			}

			void visitUsing(const pst::Using& using_stmt) override {
				auto names      = using_stmt.getPointed();
				auto lookup_res = lookupChain(ctx, LookupChainKey{ names, scope(key), false });
				CORE_ASSERT(
					lookup_res.hasValue() && not lookup_res.value().empty(),
					"Using points to something that does not exists or is empty"
				);
				auto ret = ctx.query<QueryLinkedScope>({ lookup_res.value().back() });
				output(ret);
			}

			void visitImport(const pst::Import& import_stmt) override {
				// @TODO: proper error handling
				auto imported_module = frontend::getRelativeModule(
										   ctx, module(scope(key)), import_stmt.getModulePath()
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
				return queryBodyCodeScopeFor(ctx, key.ref->stmtCast());


			// Special cases for "wildcards":
			case SymbolKind::Using:
			case SymbolKind::Import: {
				QueryLinkedScopeVisitor visitor(ctx, key);
				key.ref->stmtCast()->acceptVisitor(visitor);
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

			auto alias_definition = dynamic_cast<const pst::Alias*>(&*getSymRef(key)->pst_element);

			bool       first_symbol = true;
			SymbolList result;
			for (auto pointed: alias_definition->getPointed()) {
				auto pointed_symbol_lookup
					= first_symbol
				        ? ctx.query<QueryLookupInScopeAndParents>({ scope(key), pointed, false })
				        : ctx.query<QueryLookupInSymbol>({ result.back(), pointed, false });
				auto&& path = pointed_symbol_lookup->getAsSingle();
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
		// @todo HOUT 2.0 move this to comp_time on integers

		struct EvaluateHoutExprVisitor: public code::HoutExprVisitor {
			Context&                                  ctx;
			errors::HResult<i64 COMMA errors::Failed> result;

			EvaluateHoutExprVisitor(Context& ctx): ctx(ctx) {}

			static errors::HResult<i64, errors::Failed>
				evaluateExpr(Context& ctx, const code::Expr& expr) {
				EvaluateHoutExprVisitor visitor(ctx);
				expr.acceptVisitor(visitor);
				return visitor.result;
			}

			void visitLiteralValueExpr(const code::LiteralValueExpr& expr) override {
				result = expr.value;
			}

			void visitIdentifierExpr(const code::IdentifierExpr& expr) override {
				result = *ctx.query<QueryConstValueOf>(expr.symbol);
			}

			void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) override {
				auto lhs_result = evaluateExpr(ctx, *expr.lhs);
				if (lhs_result.hasError()) {
					result = lhs_result;
					return;
				}
				auto rhs_result = evaluateExpr(ctx, *expr.rhs);
				if (rhs_result.hasError()) {
					result = rhs_result;
					return;
				}
				i64 lhs_value = lhs_result.value(), rhs_value = rhs_result.value();
				if (expr.op.value == "+")
					result = lhs_value + rhs_value;
				else if (expr.op.value == "-")
					result = lhs_value - rhs_value;
				else if (expr.op.value == "*")
					result = lhs_value * rhs_value;
				else if (expr.op.value == "/")
					result = lhs_value / rhs_value;
				else if (expr.op.value == "%")
					result = lhs_value % rhs_value;
				else if (expr.op.value == "**")
					result = std::pow(lhs_value, rhs_value);
				else {
					result = errors::HError(errors::Failed());
					throw base::NotYetImplemented(
						"Evaluation of different than '+-*/%**' binary operators is not "
						"implemented yet"
					);
				}
			}

			void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) override {
				result = evaluateExpr(ctx, *expr.expr);
				if (result.hasError()) return;
				i64 result_value = result.value();
				if (expr.op.value == "-" && expr.prefix)
					result = -result_value;
				else {
					// Not implemented yet
					result = errors::HError(errors::Failed());
				}
			}

			void visitParenthesisExpr(const code::ParenthesisExpr& expr) override {
				result = evaluateExpr(ctx, *expr.inner);
			}

			void visitKeywordExpr([[maybe_unused]] const code::KeywordExpr& expr) override {
				throw base::NotYetImplemented("Evaluation of keyword values is not implemented yet"
				);
			}

			void visitTupleTypeConstructorExpr(
				[[maybe_unused]] const code::TupleTypeConstructorExpr& expr
			) override {
				throw base::NotYetImplemented("Evaluation of tuple values is not implemented yet");
			}

			void visitVariantTypeConstructorExpr(
				[[maybe_unused]] const code::VariantTypeConstructorExpr& expr
			) override {
				throw base::NotYetImplemented("Evaluation of variant values is not implemented yet"
				);
			}

			void visitLinkedIdentifierExpr(const code::LinkedIdentifierExpr& expr) override {
				result = *ctx.query<QueryConstValueOf>(expr.symbols.back());
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Const, "SymID is not a Const");

			const auto const_symbol = dynamic_cast<const pst::Const*>(&*getSymRef(key)->pst_element);

			auto eval = ctx.query<QueryHoutOfExpr>({ const_symbol->getValue() });
			if (eval.hasError()) return errors::HError(errors::Failed());
			return EvaluateHoutExprVisitor::evaluateExpr(ctx, *eval.value());
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryConstValueOf);

	struct IMPLEMENT_QUERY(QueryTypeOfSymbol, QueryType_Result) {
		class PstStmtVisitor_GetTypeOf final: public pst::PstStmtVisitorPanicky {
			Context& ctx;

			void setTypeOfSymbol(const tsh::TypeInfo& type) {
				if (symbol_type_info.has_value())
					CORE_PANIC("Attempted to set type of symbol in visitor a second time.");
				symbol_type_info = type;
			}

			void setTypeOfSymbol(MCRef<pst::ExprElement> expr) {
				auto tp = ctx.query<EvalExprToType>({ expr });
				if (tp.hasValue()) setTypeOfSymbol(tp.value());
			}

		public:
			PstStmtVisitor_GetTypeOf(Context& ctx): ctx(ctx) {}

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

				for (auto param: *params) {
					// @TODO HOUT 2.0: we should create symbol from parameter here, and just get
					// type of symbol. its not trivial, since parameter symbols don't exist yet
					auto parse_type_res = ctx.query<EvalExprToType>({ param->getType() });
					if (parse_type_res.hasValue()) {
						param_types.emplace_back(parse_type_res.value());
					} else {
						// @TODO: Report an error
						return;
					}
				}
				tsh::TypeInfo ret_type = ctx.query<tsh::QueryUnitType>({});
				if (ret.has_value()) {
					auto parsed = ctx.query<EvalExprToType>({ ret.value() });
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
				setTypeOfSymbol(ctx.query<tsh::QueryMetaType>({}));
			}

			void visitNamespace(const pst::Namespace&) override {
				setTypeOfSymbol(ctx.query<tsh::QueryNamespaceType>({}));
			}

			void visitImport(const pst::Import&) override {
				setTypeOfSymbol(ctx.query<tsh::QueryImportType>({}));
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto symbol_ref = getSymRef(key);

			PstStmtVisitor_GetTypeOf visitor(ctx);
			symbol_ref->pst_stmt->acceptVisitor(visitor);

			// @TODO in this PR: add support for other symbol
			
			if_opt_some(visitor.symbol_type_info, type) return type;
			return errors::HError(errors::Failed());
		}

		QUERY_AUTO_CACHE_REF;
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
			auto symbol_ref = getSymRef(key);

			PstStmtVisitor_GetTypeFromDefinition visitor(ctx, key);
			symbol_ref->pst_stmt->acceptVisitor(visitor);
			return visitor.definition_type_info.value();
		}

		QUERY_AUTO_CACHE_REF;
	};
	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeFromDefinition)

	struct IMPLEMENT_QUERY(QueryClassSymbolData, QueryClassSymbolData_Result) {
		struct ClassDataParser final: pst::PstStmtVisitorPanicky {
			base::Optional<base::StrID>                name;
			base::Optional<MCRef<pst::ExprElement>>    base_class;
			base::Optional<MCRef<pst::ImplementsList>> implements;

			void visitClass(const pst::Class& stmt) override {
				name = stmt.getName();
				if (auto base = stmt.getBase(); base != nullptr) base_class = base;
				if (auto implements = stmt.getImplements(); implements != nullptr)
					this->implements = implements;
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Class, "Symbol is not a class");

			auto class_stmt = getSymRef(key)->pst_stmt;

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
			auto class_data_parser = ClassDataParser();
			class_stmt->acceptVisitor(class_data_parser);
			class_info.name = class_data_parser.name.value();

			if_opt_some(class_data_parser.base_class, base) {
				auto tp = ctx.query<EvalExprToType>({ base });
				if (tp.hasValue()) {
					class_info.base = tp.value();
				} else {
					// @TODO: Report an error
					return errors::HError(errors::Failed());
				}
			}

			if_opt_some(class_data_parser.implements, implements) {
				for (auto&& interface: *implements) {
					auto tp = ctx.query<EvalExprToType>({ interface });
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

		QUERY_AUTO_CACHE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassSymbolData);

	struct
		IMPLEMENT_QUERY(QueryHOUTExprTreeOfSym, errors::HResult<base::Box<code::Expr> COMMA errors::Failed>) {
		struct PstStmtVisitor_GetHOUTExprTree final: public pst::PstStmtVisitorPanicky {
			Context& ctx;
			ScopeID  scope;

			errors::HResult<base::Box<code::Expr>, errors::Failed> expr_tree;

			void setExprTree(const MCRef<pst::ExprElement>& expr) {
				expr_tree = ctx.query<QueryHoutOfExpr>({ expr });
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

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHOUTExprTreeOfSym);
}

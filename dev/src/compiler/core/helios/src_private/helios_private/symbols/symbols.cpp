#include "symbols.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup_chain.hpp>
#include <helios_private/pst_layer/pst_parent.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/standard_query/query_impl.hpp>
#include <string_id/string_id.hpp>

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
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryLinkedScope, SymID, query::QResult<ScopeID>, ({}));

	bool isWildcard(SymID id) { return getSymRef(id)->common.is_wildcard; }

	bool isAlias(SymID id) { return getSymRef(id)->common.is_alias; }

	base::StrID name(SymID id) { return getSymRef(id)->common.name; }

	// @TODO: #895 Reevaluate this helper when entry points become explicit.
	bool isGlobalFun(SymID id) {
		CORE_ASSERT(
			getSymRef(id)->common.kind == SymbolKind::FunctionDeclaration
				|| getSymRef(id)->common.kind == SymbolKind::Function,
			"Not a function."
		);

		return scopeDepth(scope(id)) == 1;
	}

	bool isGlobalVar(query::Context& ctx, SymID id) {
		CORE_ASSERT(getSymRef(id)->common.kind == SymbolKind::Variable, "Not a variable.");

		// We go up the PST until we find a statement that determines whether the variable is global
		// or not.
		return std::invoke(
			[&ctx](this auto self, const pst::Access<pst::LangElement>& el) -> bool {
				switch (el->getElementKind()) {
				// Variables inside Top-level and namespace are global:
				case pst::ElementKind::TopLevel:
				case pst::ElementKind::Namespace:
					return true;

				// Variables inside classes, functions and some statements are not global:
				case pst::ElementKind::Class:
				case pst::ElementKind::Fun:
				case pst::ElementKind::ClassBlock:
				case pst::ElementKind::ClassMethod:
				case pst::ElementKind::ClassSpecial:
				case pst::ElementKind::ClassSpecifierBlock:
				case pst::ElementKind::If:
				case pst::ElementKind::While:
				case pst::ElementKind::For:
					return false;

				// For other elements we go up the PST tree:
				case pst::ElementKind::CodeBlock:
				case pst::ElementKind::CodeBlockOrStmt:
				case pst::ElementKind::Variable:
				case pst::ElementKind::Expand:
				case pst::ElementKind::StmtSpecifier:
				case pst::ElementKind::SpecifierBlock:
				case pst::ElementKind::Block:
				case pst::ElementKind::ExprElement:
				case pst::ElementKind::ExprHolder:
				case pst::ElementKind::ExprStmt: {
					auto pst_parent = getPSTElementParent(ctx, el);

					CORE_ASSERT(
						pst_parent.isLangElement(),
						"Non TopLevel elements should always have a pst or an expand parent"
					);
					return self(pst_parent.getAsLangElement().unlock(ctx));
				}
				default:
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						base::strConcat(
							"Global variable detection is not implemented for variables inside "
							"elements of kind: ",
							el->elementType()
						),
						el->getStablePosition()
					));
					query::throwFailed();
					CORE_UNREACHABLE();
				}
			},
			getSymRef(id)->getPSTData()->getElement().unlock(ctx)
		);
	}

	SymbolKind kind(SymID id) { return getSymRef(id)->common.kind; }

	ScopeID scope(SymID id) { return getSymRef(id)->getScope(); }

	bool shouldLinkOnce(SymID id) {
		variant_match(getSymRef(id)->other) {
			variant_case_novalue(PstSymbolData) { return false; }
			variant_case(defgen::GeneratedSymbolData, gen_data) {
				variant_match(gen_data.data) {
					variant_case_novalue(defgen::GeneratedSymbolData::BuiltinOperator) {
						// BuiltinOperators (better name pending) are those functions which
						// are defined in C++, and will need to be declared with external linkage.
						return false;
					}
					variant_default { return true; }
				}
			}
			variant_default { CORE_PANIC("Unhandled symbol kind"); }
		}
		CORE_UNREACHABLE();
	}

	base::Optional<ScopeID> maybeScope(SymID id) {
		variant_match(getSymRef(id)->other) {
			variant_case(PstSymbolData, pst_data) { return pst_data.scope; }
			variant_case(defgen::GeneratedSymbolData, gen_data) { return gen_data.maybeScope(); }
			variant_default { CORE_PANIC("Unhandled symbol kind"); }
		}
		CORE_UNREACHABLE();
	}

	base::Optional<pst::Access<pst::Stmt>> stmt(query::Context& ctx, SymID id) {
		return getSymRef(id)->stmtCast(ctx);
	}

	base::Optional<pst::AccessLocked<pst::LangElement>> symbolPst(SymID id) {
		return getSymRef(id)->getPSTDataOpt().map([](auto pst_data) {
			return pst_data->getElement();
		});
	}

	base::Optional<pst::AccessLocked<pst::LangElement>> maybeSymbolPst(SymID id) {
		return getSymRef(id)->getPSTDataOpt().map([](CRef<PstSymbolData> data) {
			return data->getElement();
		});
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

			auto name = stmt.value()->getDeclSymbolIdentifier();
			if (!name.has_value()) continue;

			auto id = name->unlock(ctx)->unwrap();

			if (!out.empty())
				out = base::strConcat(id, " -> ", out);
			else
				out = id.str();

			// Get the parent of the current pst element
		} while ((pst = pst.value().unlock(ctx)->getParent()));
		auto module_name = compiler::frontend::moduleName(module(scope(sym)));
		out              = base::strConcat(module_name.str(), " -> ", out);

		return out;
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
	SymbolData makeSymbolFromStatement(
		query::Context& ctx, ScopeID scope, pst::Access<pst::Stmt> stmt
	) {
		// @TODO: change this function to visitor to avoid dynamic_casts

		PstSymbolData pst_data(scope, stmt->getHash());

		switch (stmt->getStmtKind()) {
		case pst::StmtKind::Fun: {
			auto function = stmt.dynamicCast<pst::Fun>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name = function->getName().unlock(ctx)->unwrap(),
					.kind = SymbolKind::Function,
				},
				pst_data
			);
		}
		case pst::StmtKind::FunDecl: {
			auto function = stmt.dynamicCast<pst::FunDecl>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name = function->getName().unlock(ctx)->unwrap(),
					.kind = SymbolKind::FunctionDeclaration,
				},
				pst_data
			);
		}
		case pst::StmtKind::Namespace: {
			auto namespace_stmt = stmt.dynamicCast<pst::Namespace>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name = namespace_stmt->getName().unlock(ctx)->unwrap(),
					.kind = SymbolKind::Namespace,
				},
				pst_data
			);
		}
		case pst::StmtKind::Const: {
			auto const_stmt = stmt.dynamicCast<pst::Const>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name = const_stmt->getName().unlock(ctx)->unwrap(),
					.kind = SymbolKind::Const,
				},
				pst_data
			);
		}
		case pst::StmtKind::Class: {
			auto class_stmt = stmt.dynamicCast<pst::Class>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name = class_stmt->getName().unlock(ctx)->unwrap(),
					.kind = SymbolKind::Class,
				},
				pst_data
			);
		}
		case pst::StmtKind::Alias: {
			auto alias = stmt.dynamicCast<pst::Alias>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name     = alias->getName().unlock(ctx)->unwrap(),
					.kind     = SymbolKind::Alias,
					.is_alias = true,
				},
				pst_data
			);
		}
		case pst::StmtKind::Using: {
			auto  using_stmt  = stmt.dynamicCast<pst::Using>().value();
			auto  pointed     = using_stmt->getPointed().unlock(ctx);
			bool  is_wildcard = pointed->getStar();
			usize size        = pointed->numberOfNames();

			std::vector<tpc::Identifier> target(size);
			for (usize i = 0; i < size; i++)
				target[i] = { .value = pointed->getNameByIndex(i).unlock(ctx)->unwrap() };

			return SymbolData::makePSTSymbolData(
				{
					.name
					= is_wildcard
			            ? base::StrID(
							  base::strConcat("<WILDCARD USING> ", target.front().value).c_str()
						  )
			            : target.back().value,
					.kind        = SymbolKind::Using,
					.is_wildcard = is_wildcard,
					.is_alias    = true,
				},
				pst_data
			);
		}
		case pst::StmtKind::Variable: {
			auto variable = stmt.dynamicCast<pst::Variable>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name        = variable->getName().unlock(ctx)->unwrap(),
					.kind        = SymbolKind::Variable,
					.is_wildcard = false,
					.is_alias    = false,
				},
				pst_data
			);
		}
		case pst::StmtKind::Import: {
			// @TODO: #2791 finish this, note that this may require bigger refactor to unify the
			// logic with other similar constructs (e.g. usings), and because imports may introduce
			// multiple names now. We could extend wildcard machinery to keep the general assumption
			// of one-stmt=one-symbol while handling the above.
			auto import       = stmt.dynamicCast<pst::Import>().value();
			auto import_chain = import->getImportChain().unlock(ctx);
			if (auto import_as = import_chain.dynamicCast<pst::ImportIdentifierAs>()) {
				base::StrID name;
				if (import_as.value()->isImportAs())
					name = import_as.value()->asWhat().value().unlock(ctx)->unwrap();
				else {
					usize count = import_as.value()->numberOfNames();
					name = import_as.value()->getNameByIndex(count - 1).unlock(ctx)->unwrap();
				}

				return SymbolData::makePSTSymbolData(
					{
						.name        = name,
						.kind        = SymbolKind::Import,
						.is_wildcard = false,
						.is_alias    = false,
					},
					pst_data
				);
			} else if (auto import_star = import_chain.dynamicCast<pst::ImportStarHides>()) {
				// import a.b.c.*;
				usize count = import_star.value()->numberOfNames();
				auto  name  = import_star.value()->getNameByIndex(count - 1).unlock(ctx)->unwrap();
				if (import_star.value()->isImportHides()) {
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						"Import chains of type `ImportStarHides` are not yet supported in "
						"makeSymbolFromStatement",
						import_star.value()->getStablePosition()
					));
				}
				return SymbolData::makePSTSymbolData(
					{
						.name        = name,
						.kind        = SymbolKind::Import,
						.is_wildcard = true,
						.is_alias    = false,
					},
					pst_data
				);
			} else if (auto import_nested = import_chain.dynamicCast<pst::ImportNested>()) {
				// import a.b.c(...);
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Import chains of type `ImportNested` are not yet supported in "
					"makeSymbolFromStatement",
					import_nested.value()->getStablePosition()
				));
				usize count = import_nested.value()->numberOfNames();
				auto  name = import_nested.value()->getNameByIndex(count - 1).unlock(ctx)->unwrap();
				return SymbolData::makePSTSymbolData(
					{
						.name        = name,
						.kind        = SymbolKind::Import,
						.is_wildcard = false,
						.is_alias    = false,
					},
					pst_data
				);
			} else {
				throw base::NotYetImplemented(
					"Not handled type of import chain in makeSymbolFromStatement"
				);
			}
		}
		case pst::StmtKind::Method: {
			auto method = stmt.dynamicCast<pst::Method>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name = method->getName().unlock(ctx)->unwrap(),
					.kind = SymbolKind::Method,
				},
				pst_data
			);
		}
		case pst::StmtKind::Field: {
			auto field = stmt.dynamicCast<pst::Field>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name      = field->getName().unlock(ctx)->unwrap(),
					.kind      = SymbolKind::Field,
					.dependent = true,
				},
				pst_data
			);
		}
		case pst::StmtKind::Constructor: {
			auto constructor = stmt.dynamicCast<pst::Constructor>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name = constructor->getIdentifier()
			                  ? constructor->getIdentifier()->unlock(ctx)->unwrap()
			                  : base::StrID("create"),
					.kind = SymbolKind::Constructor,
				},
				pst_data
			);
		}
		case pst::StmtKind::CopyConstructor: {
			// left for code consistency
			[[maybe_unused]]
			auto constructor
				= stmt.dynamicCast<pst::CopyConstructor>().value();
			return SymbolData::makePSTSymbolData(
				{
					.name = lang_def::keywordToStr(lang_def::Keyword::Copy),
					.kind = SymbolKind::Constructor,
				},
				pst_data
			);
		}
		case pst::StmtKind::Destructor: {
			return SymbolData::makePSTSymbolData(
				{
					.name = base::StrID("destroy"),
					.kind = SymbolKind::Destructor,
				},
				pst_data
			);
		}
		case pst::StmtKind::CodeDecl: {
			// CodeDecl include things like named ifs, whiles, fors and code blocks.
			// Note that this function should only be called if the statement creates a symbol, so
			// we can assume that it is only named ones.
			return SymbolData::makePSTSymbolData(
				{
					.name = stmt->getDeclSymbolIdentifier()->unlock(ctx)->unwrap(),
					.kind = SymbolKind::NamedCodeElement,
				},
				pst_data
			);
		}
		default:
			break;
		}
		[[maybe_unused]] auto stmt_ptr = &*stmt;
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
	SymbolData makeSymbolFromPSTElement(
		ScopeID scope, pst::Access<pst::LangElement> element, query::Context& ctx
	) {
		if (auto parameter_opt = element.dynamicCast<pst::Param>()) {
			auto parameter = parameter_opt.value();
			return SymbolData::makePSTSymbolData(
				{
					.name = parameter->getName().unlock(ctx)->unwrap(),
					.kind = SymbolKind::Parameter,
				},
				PstSymbolData(scope, element->getHash())
			);
		}
		CORE_PANIC("Not handled PST element in makeSymbolFromPSTElement");
	}

	struct IMPLEMENT_QUERY(QuerySymbolOfSTMT, query::QResult<SymbolData>) {
		/**
		 * @brief Return the scope, that symbol created from given PST element
		 * Should be in.
		 * @note This has to be consistent with QuerySymbolsInScope
		 * @TODO: #2407 this could take unlocked Access
		 */
		static ScopeID getPSTElementParentScope(
			query::Context& ctx, pst::AccessLocked<pst::LangElement> element
		) {
			// Note: This has to be consistent with QuerySymbolsInScope logic.
			// @TODO: #2397 maybe move it into a single place

			auto unlocked = element.unlock(ctx);
			auto parent   = getPSTElementParent(ctx, unlocked);

			// We should never hit an element without PST parent here,
			// since it would be a non-expand root element (i.e. TopLevel element), and those don't
			// have symbols.
			CORE_ASSERT(
				parent.isLangElement(), "PST element without LangElement parent in QuerySymbolOfSTMT"
			);
			return ctx.query<QueryPrimaryCodeScopeFor>(parent.getAsLangElement());
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto scope = getPSTElementParentScope(ctx, key.element);
			if (key.element.unlock(ctx)->getElementKind() == pst::ElementKind::NonClassStmt) {
				// @TODO: #2087 remove this branch, when non-class statements will be properly supported.
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Non-class statements inside classes are not supported yet.",
					key.element.unlock(ctx)->getStablePosition(),
					"",
					"here"
				));
				return query::Failed();
			} else if (auto stmt = key.element.unlock(ctx).dynamicCast<pst::Stmt>())
				return PResult{ makeSymbolFromStatement(ctx, scope, stmt.value()) };
			else
				return PResult{ makeSymbolFromPSTElement(scope, key.element.unlock(ctx), ctx) };
		}

		QUERY_AUTO_CACHE_CONSTRUCT_BY_LAMBDA([](CRef<query::QResult<SymbolData>> presult) -> QResult {
			if (presult->hasFailed())
				return query::Failed();
			else
				return SymID{ &presult->valueOrPanic() };
		})

	private:
		/**
		 * @brief This is a helper function for getAllHeliosSymbols.
		 * Use only inside that function (and only for debug/test purposes)!
		 */
		static std::vector<SymID> getAllCachedSymbols() {
			// This implementation is fragile, adjust if needed.

			std::vector<SymID> out;

			for (auto& [key, cache_entry]: cache) {
				if (cache_entry.data.hasFailed()) continue;

				out.emplace_back(SymID{ &cache_entry.data.valueOrPanic() });
			}
			return out;
		}

		// for getAllCachedSymbols:
		friend std::vector<SymID> compiler::helios::getAllHeliosSymbols();
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolOfSTMT);

	struct IMPLEMENT_QUERY(QueryLookupInSymbol, query::QResult<LookupResult>) {
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

				UNPACK_QRESULT(auto linked_scope =, ctx.query<QueryLinkedScope>(key.symbol));
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

	struct IMPLEMENT_QUERY(QueryLinkedScope, query::QResult<ScopeID>) {
		struct QueryLinkedScopeVisitor final: pst::PstVisitorEmpty {
			query::Context& ctx;
			QKey            key;

			QueryLinkedScopeVisitor(query::Context& ctx, QKey key): ctx(ctx), key(key) {}

			base::Optional<query::QResult<ScopeID>> result_scope;

			void output(query::QResult<ScopeID> out) {
				CORE_ASSERT(result_scope.empty(), "Output already set");
				result_scope.emplace(out);
			}

			void visitUsing(pst::Access<pst::Using> using_stmt) final {
				auto                         pointed = using_stmt->getPointed().unlock(ctx);
				usize                        size    = pointed->numberOfNames();
				std::vector<tpc::Identifier> pointed_to_names(size);
				for (usize i = 0; i < size; i++) {
					pointed_to_names[i]
						= { .value = pointed->getNameByIndex(i).unlock(ctx)->unwrap() };
				}

				auto lookup_res = lookupChain(
					ctx,
					LookupChainKey{ .names       = pointed_to_names,
				                    .begin_scope = scope(key),
				                    .params      = { .with_wildcards = false } }
				);
				CORE_ASSERT(
					lookup_res.hasValue() && not lookup_res.valueOrThrow().empty(),
					"Using points to something that does not exists or is empty"
				);
				auto ret = ctx.query<QueryLinkedScope>({ lookup_res.valueOrThrow().back() });
				output(ret);
			}

			void visitImport(pst::Access<pst::Import> import_stmt) final {
				// @TODO: proper error handling

				auto import_stmt_ptr = import_stmt.dynamicCast<pst::Import>().value();
				auto import_chain    = import_stmt_ptr->getImportChain().unlock(ctx);

				std::vector<base::StrID> module_path;

				auto unlock_all_names = [&](auto import_chain_casted) {
					usize                    size = import_chain_casted->numberOfNames();
					std::vector<base::StrID> names(size);
					for (usize i = 0; i < size; i++)
						names[i] = import_chain_casted->getNameByIndex(i).unlock(ctx)->unwrap();
					return names;
				};

				// Handle different import chain types
				if (auto import_as = import_chain.dynamicCast<pst::ImportIdentifierAs>()) {
					auto names  = unlock_all_names(import_as.value());
					module_path = std::vector<base::StrID>{ names.begin(), names.end() };
				} else if (auto import_star = import_chain.dynamicCast<pst::ImportStarHides>()) {
					auto names  = unlock_all_names(import_star.value());
					module_path = std::vector<base::StrID>{ names.begin(), names.end() };
				} else {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						"Unknown import chain type.", import_stmt->getStablePosition()
					));
					output(query::Failed());
					return;
				}

				auto maybe_imported_module
					= frontend::getRelativeModule(ctx, module(scope(key)), module_path);

				if (!maybe_imported_module.has_value()) {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						"Module not found.", import_stmt->getStablePosition()
					));
					output(query::Failed());
					return;
				}

				// Here we don't access just root scope, because root scopes are currently empty:
				auto linked_scope
					= queryRootScopeOfMainModuleFile(ctx, maybe_imported_module.value());

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
				key.ref->getPSTData()->getElement().unlock(ctx)->acceptVisitor(visitor);
				return visitor.result_scope.value();
			}
			default: {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"Linked scope for this symbol kind is not implemented yet: ",
						key.ref->common.kind
					),
					stmt(ctx, key.ref).value()->getStablePosition()
				));
				return query::Failed();
			}
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
				auto dotted = getSymRef(key)
				                  ->getPSTData()
				                  ->getElement()
				                  .unlock(ctx)
				                  .dynamicCast<pst::Using>()
				                  .value()
				                  ->getPointed()
				                  .unlock(ctx);
				pointed_chain.resize(dotted->numberOfNames());
				for (usize i = 0; i < dotted->numberOfNames(); i++)
					pointed_chain[i] = { .value = dotted->getNameByIndex(i).unlock(ctx)->unwrap() };
			} else if (kind(key) == SymbolKind::Alias) {
				auto dotted = getSymRef(key)
				                  ->getPSTData()
				                  ->getElement()
				                  .unlock(ctx)
				                  .dynamicCast<pst::Alias>()
				                  .value()
				                  ->getPointed()
				                  .unlock(ctx);
				pointed_chain.resize(dotted->numberOfNames());
				for (usize i = 0; i < dotted->numberOfNames(); i++)
					pointed_chain[i] = { .value = dotted->getNameByIndex(i).unlock(ctx)->unwrap() };
			} else {
				return SymbolList{ { key } };
			}

			UNPACK_QRESULT_MOVE(
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

	struct IMPLEMENT_QUERY(QueryConstValueOf, query::QResult<ctv::CompileTimeValue>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Const, "SymID is not a Const");

			// Get the const's data
			const auto pst = getSymRef(key)
			                     ->getPSTData()
			                     ->getElement()
			                     .unlock(ctx)
			                     .dynamicCast<pst::Const>()
			                     .value();
			const auto type = ctx.query<QueryTypeOfSymbol>(key)->valueOrThrow();

			// Get the coerced HOUT expression
			const auto hout_qresult = getHoutOfExprWithExpectedType(
				ctx, pst->getValue().value().unlock(ctx)->getExpr(), type
			);
			if (hout_qresult.hasFailed()) return query::Failed();

			// Evaluate the HOUT expression at compile-time
			auto ctv
				= ctx.query<QueryEvaluateHOUTExpression>({ hout_qresult.valueOrThrow().ref() });
			if (ctv.hasFailed()) return query::Failed();
			return ctv.valueOrThrow();
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
			if (std::holds_alternative<defgen::GeneratedSymbolData>(getSymRef(key)->other)) {
				// Generated symbols have no specifiers (for now)
				return {};
			}

			auto pst_element = getSymRef(key)->getPSTData()->getElement().unlock(ctx);

			// SpecifierBlock only has a "CodeBlock" child, which can has "Stmt" children.
			//
			// The Class situation is a bit more complicated
			// @TODO: #1535 Fix/figure out class handling
			while (true) {
				if (auto as_stmt = pst_element.dynamicCast<pst::Stmt>()) {
					// Can swap to append range when g++ 15 is more commonly available
					auto to_add = as_stmt.value()->getSpecifiers();
					specifiers.insert(
						specifiers.end(),
						std::make_move_iterator(to_add.begin()),
						std::make_move_iterator(to_add.end())
					);
				}
				if (auto result_stmt = getAncestor(
						ctx, pst_element, pst::ElementKind::CodeBlock, pst::ElementKind::SpecifierBlock
					)) {
					pst_element = *std::move(result_stmt);
				} else if (auto result_block = getAncestor(
							   ctx,
							   pst_element,
							   pst::ElementKind::ClassBlock,
							   pst::ElementKind::ClassSpecifierBlock
						   )) {
					pst_element = *std::move(result_block);
				} else {
					break;
				}
			}

			return specifiers;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySpecifiersOfSymbol);

	namespace defgen {
		base::Bit256 KeyFor_QueryGeneratedSymbol::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				std::hash<base::StrID>()(name), generated_symbol_data.queryUnstablePerfectHash()
			);
		}

		struct IMPLEMENT_QUERY(QueryGeneratedSymbol, SymbolData) {
			static auto provide(Context&, QKey key) -> PResult {
				return SymbolData::makeGeneratedSymbol(key.name, key.generated_symbol_data);
			}

			QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF_IGNORE_CONSTRUCTIBILITY_CHECK

		private:
			/**
			 * @brief This is a helper function for getAllHeliosSymbols.
			 * Use only inside that function (and only for debug/test purposes)!
			 */
			static std::vector<SymID> getAllCachedSymbols() {
				// This implementation is fragile, adjust if needed.

				std::vector<SymID> out;

				for (auto& [key, cache_entry]: cache)
					out.emplace_back(QResult{ &cache_entry.data });
				return out;
			}

			// for getAllCachedSymbols:
			friend std::vector<SymID> compiler::helios::getAllHeliosSymbols();
		};

		QUERY_IMPLEMENTATION_BOILERPLATE(QueryGeneratedSymbol);
	}

	struct IMPLEMENT_QUERY(QueryDirectFunctionCalls, query::QResult<std::vector<SymID>>) {
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

			void visitBlockStmt(const code::BlockStmt& stmt) override {
				for (const auto& sub_stmt: stmt.body.statements) sub_stmt->acceptVisitor(*this);
			}

			void visitVariableStmt(const code::VariableStmt& stmt) override {
				stmt.initial_value->acceptVisitor(*this);
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
				for (const auto& sub_expr: expr.comparisons) sub_expr->acceptVisitor(*this);
			}

			void visitTupleExpr(const code::TupleExpr& expr) override {
				// Tuple expression is equivalent to a function call to the tuple constructor in MIR
				called_functions.insert(expr.tuple_ctor_symbol);
				for (const auto& sub_expr: expr.elements) sub_expr->acceptVisitor(*this);
			}

			void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& expr
			) override {
				for (const auto& sub_expr: expr.subtypes) sub_expr->acceptVisitor(*this);
			}

			void visitLiftToTypeExpr(const code::LiftToTypeExpr& expr) override {
				expr.value_expr->acceptVisitor(*this);
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Function || kind(key) == SymbolKind::Method
					|| kind(key) == SymbolKind::FunctionDeclaration,
				"Query function dependencies called on non-function symbol"
			);
			if (kind(key) == SymbolKind::FunctionDeclaration) {
				// For function declarations we return empty dependencies, since they don't have a body.
				return std::vector<SymID>{};
			}


			auto collect_deps = [&]() {
				const auto& fun_hout_result = ctx.query<QueryCodeOfFun>(key)->valueOrThrow();
				const auto& function_body   = fun_hout_result.body;

				HoutFunctionCallCollector visitor;
				for (const auto& stmt: function_body->statements) stmt->acceptVisitor(visitor);
				return std::ranges::to<std::vector<SymID>>(visitor.called_functions);
			};

			variant_match(getSymRef(key)->other) {
				variant_case_novalue(PstSymbolData) {
					// Just a PST function.
					return collect_deps();
				}
				variant_case(defgen::GeneratedSymbolData, gsd_data) {
					variant_match(gsd_data.data) {
						variant_case_novalue(defgen::GeneratedSymbolData::BuiltinOperator) {
							// Builtin operators have no dependencies
							return {};
						}
					}
					CORE_ASSERT(
						gsd_data.getType(ctx).getType().getKind() == tsh::Kind::Function,
						"QueryDirectFunction calls called on a non-function symbol"
					);
					return collect_deps();
				}
				variant_default { CORE_UNREACHABLE(); }
			}

			CORE_UNREACHABLE();
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDirectFunctionCalls);

	struct IMPLEMENT_QUERY(QueryTransitiveFunctionCalls, query::QResult<std::vector<SymID>>) {
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

				Ref direct_dependencies
					= &ctx.query<QueryDirectFunctionCalls>(current_func)->valueOrThrow();

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

	std::vector<SymID> getAllHeliosSymbols() {
		// this implementation is fragile, adjust if needed

		CORE_ASSERT(
			!query::Context::areWeInsideQuery(), "getAllHeliosSymbols called from within query!"
		);

		auto pst_symbols = ImplementationOf_QuerySymbolOfSTMT::getAllCachedSymbols();
		auto generated_symbols
			= defgen::ImplementationOf_QueryGeneratedSymbol::getAllCachedSymbols();

		std::vector<SymID> output;
		output.reserve(pst_symbols.size() + generated_symbols.size());

		output.insert(output.end(), pst_symbols.begin(), pst_symbols.end());
		output.insert(output.end(), generated_symbols.begin(), generated_symbols.end());

		return output;
	}
}

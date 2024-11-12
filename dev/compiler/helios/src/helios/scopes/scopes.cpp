#include "scopes.hpp"

#include <base/maps.hpp>
#include <base/stable_container.hpp>
#include <base/stable_hashmap.hpp>
#include <base/str_utils.hpp>
#include <base/string_id.hpp>
#include <base/exceptions.hpp>

#include <query_framework/query_impl.hpp>

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>

#include <pst_parser/lang_parser_state.hpp>
#include <pst_parser/pst_visitor.hpp>

#include "../lookup_result.hpp"
#include "../pst_walkers.hpp"
#include "../symbols/symbols.hpp"

namespace compiler::helios {

	struct ScopeData final {
		// adapted from hir:

		// created on startup:
		std::optional<ScopeID> parent;

		// base::StrID name; ///< for debug
		bool is_root = false;

		/**
		 * @brief PST element for which the scope was created.
		 * Empty for root scope.
		 */
		base::Optional<PstRef<pst::LangElement>> related_pst_element;

		/**
		 * @brief Module, the scope was defined in
		 */
		frontend::ModuleID parent_module;

		// cache entries:
		// in the future we might need separation for: direct symbols, expanded symbols
		// in this system scope is no longer closed/open as we think of it as a pure-value object
		// any lookup in the scope requires calculation of symbols witch itself is done only once!
		base::Optional<query::CacheEntry<SymbolList>> symbols;

		u64 depth;


		// This delete is important, to prevent any copy of scope data:
		// ScopeData(const ScopeData&)            = delete;
		// ScopeData& operator=(const ScopeData&) = delete;
	};

	struct ScopeAccess_Functor final {
		static auto get(ScopeID id) { return id.ref; }

		static auto idOf(Ref<ScopeData> ref) { return ScopeID(ref); }
	};

	auto getScopeRef(ScopeID id) { return ScopeAccess_Functor::get(id); }

	base::Optional<ScopeID> parent(ScopeID id) {
		auto ref = getScopeRef(id);
		if (ref->is_root) {
			return {};
		} else {
			CORE_ASSERT(ref->parent.has_value(), "Non root scope has no parent.");
			return ref->parent.value();
		}
	}

	frontend::ModuleID module(ScopeID id) { return getScopeRef(id)->parent_module; }

	u64 scopeDepth(ScopeID id) { return getScopeRef(id)->depth; }

	namespace {
		base::StableVector<ScopeData> scope_table;

		template<class... T>
		auto putInScopeTable(T&&... args) {
			auto key = scope_table.emplaceBack(std::forward<T>(args)...);
			return scope_table.getRef(key).value();
		}
	}

	std::vector<ScopeID> getAllHeliosScopes() {
		std::vector<ScopeID> out;
		for (auto& scope_data: scope_table)
			out.emplace_back(ScopeAccess_Functor::idOf(scope_data.refMut()));
		return out;
	}

	struct IMPLEMENT_QUERY(QueryRootScopeOf, ScopeID) {
		static auto provide(Context&, QKey key) -> PResult {
			return putInScopeTable(ScopeData{
				.parent              = {},
				.is_root             = true,
				.related_pst_element = {},
				.parent_module       = key,
				.symbols             = {},
				.depth               = 0,
			});
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryRootScopeOf);

	struct IMPLEMENT_QUERY(QueryPrimaryCodeScopeFor, ScopeID) {
		inline static base::HashMap<pst::PstID, ScopeID> parent_map;

		static auto provide(Context& ctx, QKey element) -> PResult {
			ScopeID parent = element.base_element->getParent().has_value()
			                   ? ctx.query<QueryPrimaryCodeScopeFor>(
								   { element.base_element->getParent().value() }
							   )
			                   : ctx.query<QueryRootScopeOf>(
								   { frontend::extendQueryModuleIDOfPST(ctx, element.base_element) }
							   );


			// simple parent sanity check:
			// it is technically not needed anymore, but it left as an additional
			// layer of bug detection.
			if (parent_map.contains(element.base_element->getID())) {
				CORE_ASSERT(
					parent_map.at(element.base_element->getID()) == parent,
					"Parent mismatch in QueryPrimaryCodeScopeFor"
				);
			} else {
				parent_map.put(element.base_element->getID(), parent);
			}

			return putInScopeTable(ScopeData{
				.parent              = parent,
				.related_pst_element = element.base_element,
				.parent_module       = module(parent),
				.symbols             = {},
				.depth               = scopeDepth(parent) + 1,
			});
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPrimaryCodeScopeFor);

	struct IMPLEMENT_QUERY(QuerySymbolsInScope, std::vector<SymID>) {
		/**
		 * @brief Makes symbols from pst::Stmt and filters out non declarations from the StmtList.
		 */
		template<std::derived_from<pst::Stmt> Stmt = pst::Stmt>
		static std::vector<SymID> filterSymbolsFromStmtList(
			query::Context& ctx, ScopeID scope, const StmtList<Stmt>& list
		) {
			std::vector<SymID> symbols;
			for (const auto& stmt: list) {
				if (stmt->isDeclaration()) {
					auto sym_id = ctx.query<QuerySymbolOfSTMT>({ scope, stmt });
					symbols.emplace_back(sym_id);
				}
			}
			return symbols;
		}

		/**
		 * @brief Gets symbols for scopes of various statements.
		 */
		struct SymbolGrabVisitor final: pst::PstStmtVisitorPanicky {
			SymbolGrabVisitor(Context& ctx, const QKey& key): ctx(ctx), key(key) {}

			base::Optional<std::vector<SymID>> out;
			Context&                           ctx;
			const QKey&                        key;

			template<class... Args>
			void output(Args&&... args) {
				CORE_ASSERT(this->out.empty(), "Output already set");
				this->out.emplace(std::forward<Args>(args)...);
			}

			void visitFun(const pst::Fun&) override {
				// Scope of "fun →()← {}"
				// @TODO: iterate function parameters and create symbols out of them
				// The problem is that currently function parameters are Expr in Pst -- this has to
				// change Variable declaration or custom element is probably a better choice
				output(std::vector<SymID>{});
			}

			void visitIf(const pst::If&) override {
				// Scope of "if →(...)← {}"
				// @TODO: check if "If" defines any variables in its condition
				// and add them here.
				output(std::vector<SymID>{});
			}

			void visitClass(const pst::Class& class_) override {
				output(
					filterSymbolsFromStmtList(ctx, key, getChildStmtsOfClassBlock(class_.getBody()))
				);
			}

			void visitNamespace(const pst::Namespace&) override {
				// This seems strange, but namespace scope is indeed empty.
				// The scope that is full is the codeblock within the namespace.
				output(std::vector<SymID>());
			}

			void visitExprStmt(const pst::ExprStmt&) override { output(std::vector<SymID>()); }

			void visitReturn(const pst::Return&) override { output(std::vector<SymID>()); }

			void visitVariable(const pst::Variable&) override { output(std::vector<SymID>()); }
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			std::vector<SymID> out;

			// @TODO: expand macros?

			if (not key.ref->related_pst_element.has_value()) {
				CORE_ASSERT(key.ref->is_root, "Non root scope without PST element!");
				return {};
			}
			auto base_element = key.ref->related_pst_element.value();

			if (base_element->isStatementAggregate()) {
				// @TODO: the error here is that this perform double scoping
				// for CodeBlocks inside CodeBlocksOrStmt
				// Hot-patch:
				// @TODO: change elementType usage to elementKind (once its implemented)
				if (base_element->elementType() == "Code Block")
					if (base_element->getParent().value()->elementType()
					    == "Code Block or Statement") {
						// hot patch currently does:
						// code block inside CodeBlockOrStmt has empty scope
						return {};
					}

				return filterSymbolsFromStmtList(ctx, key, getStmtsFromStmtAggregate(base_element));
			} else if (base_element->isStatement()) {
				SymbolGrabVisitor symbol_grab(ctx, key);
				auto              as_stmt = dynamic_cast<const pst::Stmt*>(base_element.get());
				as_stmt->acceptVisitor(symbol_grab);
				return std::move(symbol_grab.out.value());
			} else if (base_element->elementType() == "Expression") {
				// @FIXME: change the way we check the condition, by comparing enum
				// values instead of strings. Make the enum stringifiable.
				return std::vector<SymID>{};
			} else {
				CORE_PANIC("Query symbols from scope of non-statement, non-codeblock and non-expr");
			}
		}

		static auto load(QKey key) -> LoadResult {
			if (const auto& cache = key.ref->symbols) return QResWithACD{ &cache->data, cache->acd };
			return {};
		}

		static auto store(QKey key, PResult p_res, query::ACD acd) -> QResult {
			key.ref->symbols.emplace(PResWithACD{ std::move(p_res), acd });
			return &key.ref->symbols.value().data;
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolsInScope);

	struct IMPLEMENT_QUERY(QueryLookupInScope, LookupResult) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto symbol_list = ctx.query<QuerySymbolsInScope>(key.scope);

			LookupResult result{ {}, {} };

			for (const auto& sym: *symbol_list) {
				if (isWildcard(sym)) {
					if (key.with_wildcards) {
						auto wild_result = ctx.query<QueryLookupInSymbol>({ sym, key.name, true });
						if (!wild_result->isEmpty())
							result.children.push_back(wild_result->toNode(sym));
					}
				} else if (name(sym) == key.name) {
					result.leaves.push_back(sym);
				} else {
					// nothing?
				}
			}

			return result;
		}

		QUERY_AUTO_CACHE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInScope);

	struct IMPLEMENT_QUERY(QueryLookupInScopeAndParents, LookupResult) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			auto result = ctx.query<QueryLookupInScope>(key);

			if (key.scope.ref->parent.has_value()) {
				auto parent = key.scope.ref->parent.value();

				// Reverse insertion order allow for linear result concatenation instead of
				// quadratic
				LookupResult parent_result = *ctx.query<QueryLookupInScopeAndParents>(
					{ parent, key.name, key.with_wildcards }
				);

				parent_result.insert(*result);

				return parent_result;
			} else {
				return *result;
			}
		}

		QUERY_AUTO_CACHE_REF;
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInScopeAndParents);

	base::HashT KeyOf_QueryPrimaryCodeScopeFor::customPerfectHash() const {
		auto hash_1 = base_element->getID().asInt();
		return hash_1;
	}

	base::HashT KeyOf_LookupInScope::customPerfectHash() const {
		auto hash_1 = base::perfectHash(scope);
		auto hash_2 = std::hash<base::StrID>()(name);

		// @FIXME: this does not work:
		return (hash_1 * 143 + hash_2 * 7) * 2 + with_wildcards;
	}

	ScopeID extendQueryRootScopeOfMainModuleFile(query::Context& ctx, frontend::ModuleID module) {
		auto main_source_file = ctx.query<frontend::QueryMainSourceFile>(module);
		auto main_source_pst  = ctx.query<frontend::QueryFilePST>(main_source_file);

		auto main_file_root_scope
			= ctx.query<QueryPrimaryCodeScopeFor>({ main_source_pst->getRootElement() });

		return main_file_root_scope;
	}
}

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
		// created on startup:
		std::optional<ScopeID> parent;

		// base::StrID name; ///< for debug
		bool is_root = false;

		/**
		 * @brief PST element for which the scope was created.
		 * Empty for root scope.
		 */
		base::Optional<MCRef<pst::LangElement>> related_pst_element;

		/**
		 * @brief Module, the scope was defined in
		 */
		frontend::ModuleID parent_module;

		// cache entry:
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

	/**
	 * @brief A way helios creates scope for given pst element.
	 */
	enum class ElementScopeKind {
		// Has standard scope:
		Standard,

		// Inherits scope from its parent:
		Transparent,

		// Does not have a scope:
		Invalid,

		// @TODO: introduce:
		// * TransparentInvalid -- transparent for implementation, but invalid for user
		// this allows to easily implement scope parents, but disallow to get PrimaryScopes for
		// elements that don't have it.
		// For examples namespaces don't have primary scopes (which can be counterintuitive)
	};

	/**
	 * Determines scope kind for given PST element.
	 */
	ElementScopeKind getScopeKind(MCRef<pst::LangElement> element) {
		// @TODO: move it to different file?

		switch (element->getElementKind()) {
		case pst::ElementKind::TopLevel:
			return ElementScopeKind::Standard;

		case pst::ElementKind::Import:
			return ElementScopeKind::Invalid;


		// code blocks:
		case pst::ElementKind::CodeBlock: {
			auto parent_kind = element->getParent().value()->getElementKind();
			if (parent_kind == pst::ElementKind::CodeBlockOrStmt)
				return ElementScopeKind::Transparent;
			else
				return ElementScopeKind::Standard;
		}
		case pst::ElementKind::CodeBlockOrStmt:
			return ElementScopeKind::Standard;

		case pst::ElementKind::ClassBlock: {
			// This is because AccessBlocks store a ClassBlock inside.
			// Only the "top-class" ClassBlock has a scope.
			auto parent_kind = element->getParent().value()->getElementKind();
			if (parent_kind == pst::ElementKind::Class)
				return ElementScopeKind::Standard;
			else
				return ElementScopeKind::Transparent;
		}


		case pst::ElementKind::Namespace:
		case pst::ElementKind::Variable:
		case pst::ElementKind::Using:
		case pst::ElementKind::Alias:
		case pst::ElementKind::Const:
		case pst::ElementKind::Class:
		case pst::ElementKind::Action:
		case pst::ElementKind::Block:  //< note that Block != CodeBlock
		case pst::ElementKind::ClassField:
			// this is transparent, since we don't need this scope:
			return ElementScopeKind::Transparent;

		// this has to be transparent, since ClassBlock scopes
		// contain all symbols in AccessBlock's
		case pst::ElementKind::AccessBlock:
			return ElementScopeKind::Transparent;

		case pst::ElementKind::If:
		case pst::ElementKind::While:
		case pst::ElementKind::For:
		case pst::ElementKind::Fun:
		case pst::ElementKind::ClassMethod:
			return ElementScopeKind::Standard;

		case pst::ElementKind::ExprStmt:
			// note: this will be needed for lifetimes
			return ElementScopeKind::Standard;

		case pst::ElementKind::ExprElement:
			// @todo: once we have top-expressions, this should be transparent for non-tops
			return ElementScopeKind::Standard;

		case pst::ElementKind::ExprWrapper:
			return ElementScopeKind::Transparent;

		case pst::ElementKind::FunParam:
		case pst::ElementKind::ParamList:
			return ElementScopeKind::Transparent;

		case pst::ElementKind::KindNotSet:
			CORE_UNREACHABLE();

		default:
			throw base::NotYetImplemented(
				base::strConcat("PST element scope kind for: ", element->elementType())
			);
		}
		CORE_UNREACHABLE();
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
			auto element_scope_kind = getScopeKind(element.element);

			if (element_scope_kind == ElementScopeKind::Invalid) {
				auto element_ptr = &*element.element;
				CORE_PANIC(base::strConcat(
					"Scope of element for which scope does not make sense (or was not added.): ",
					typeid(*element_ptr).name()
				));
			}

			ScopeID parent
				= element.element->getParent().has_value()
			        ? ctx.query<QueryPrimaryCodeScopeFor>({ element.element->getParent().value() })
			        : ctx.query<QueryRootScopeOf>(
						{ frontend::extendQueryModuleIDOfPST(ctx, element.element) }
					);

			if (element_scope_kind == ElementScopeKind::Transparent) return parent;

			// simple parent sanity check:
			// it is technically not needed anymore, but it left as an additional
			// layer of bug detection.
			if (parent_map.contains(element.element->getID())) {
				CORE_ASSERT(
					parent_map.at(element.element->getID()) == parent,
					"Parent mismatch in QueryPrimaryCodeScopeFor"
				);
			} else {
				parent_map.put(element.element->getID(), parent);
			}

			return putInScopeTable(ScopeData{
				.parent              = parent,
				.related_pst_element = element.element,
				.parent_module       = module(parent),
				.symbols             = {},
				.depth               = scopeDepth(parent) + 1,
			});
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPrimaryCodeScopeFor);

	ScopeID queryBodyCodeScopeFor(query::Context& ctx, MCRef<pst::Stmt> stmt) {
		// note: not all cases are handled here, which is intentional.
		// We might add more in the future, but this function should remain a simple one.

		struct QueryBodyScopeVisitor: pst::PstVisitorPanicky {
			query::Context&  ctx;
			MCRef<pst::Stmt> stmt;

			QueryBodyScopeVisitor(query::Context& ctx, MCRef<pst::Stmt> stmt):
				  ctx(ctx),
				  stmt(stmt) {}

			base::Optional<ScopeID> out;

			void visitNamespace(const pst::Namespace& namespace_stmt) override {
				out = ctx.query<QueryPrimaryCodeScopeFor>({ namespace_stmt.getBody() });
			}

			void visitClass(const pst::Class& class_stmt) override {
				out = ctx.query<QueryPrimaryCodeScopeFor>({ class_stmt.getBody() });
			}
		};

		QueryBodyScopeVisitor visitor(ctx, stmt);
		stmt->acceptVisitor(visitor);
		return visitor.out.value();
	}

	struct IMPLEMENT_QUERY(QuerySymbolsInScope, std::vector<SymID>) {
		/**
		 * @brief Makes symbols from pst::Stmt and filters out non declarations from the StmtList.
		 */
		template<std::derived_from<pst::Stmt> Stmt = pst::Stmt>
		static std::vector<SymID>
			filterSymbolsFromStmtList(query::Context& ctx, const StmtList<Stmt>& list) {
			std::vector<SymID> symbols;
			for (const auto& stmt: list) {
				if (stmt->isDeclaration()) {
					auto sym_id = ctx.query<QuerySymbolOfSTMT>({ stmt });
					symbols.emplace_back(sym_id);
				}
			}
			return symbols;
		}

		/**
		 * @brief Gets symbols for scopes of various statements.
		 */
		struct SymbolGrabVisitor final: pst::PstVisitorPanicky {
			SymbolGrabVisitor(Context& ctx, const QKey& key): ctx(ctx), key(key) {}

			base::Optional<std::vector<SymID>> out;
			Context&                           ctx;
			const QKey&                        key;

			template<class... Args>
			void output(Args&&... args) {
				CORE_ASSERT(this->out.empty(), "Output already set");
				this->out.emplace(std::forward<Args>(args)...);
			}

			// here, we add only stmts, that actually have a primary scope.

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

			void visitExprStmt(const pst::ExprStmt&) override { output(std::vector<SymID>()); }
		};

		/**
		 * This is an actual implementation of the query.
		 * `provide` function simply calls it and validates output.
		 */
		static auto getSymbols(Context& ctx, QKey key) -> PResult {
			// @TODO: expand macros?

			if (not key.ref->related_pst_element.has_value()) {
				CORE_ASSERT(key.ref->is_root, "Non root scope without PST element!");
				return {};
			}
			auto base_element = key.ref->related_pst_element.value();

			if (base_element->isStatementAggregate()) {
				return filterSymbolsFromStmtList(ctx, getStmtsFromStmtAggregate(base_element));
			} else if (base_element->isStatement()) {
				// note: if this check fail, it might be that we are missing some cases
				CORE_ASSERT(
					getScopeKind(base_element) == ElementScopeKind::Standard,
					"Bad element in QuerySymbolsInScope"
				);

				SymbolGrabVisitor symbol_grab(ctx, key);
				auto              as_stmt = dynamic_cast<const pst::Stmt*>(&*base_element);
				as_stmt->acceptVisitor(symbol_grab);
				return std::move(symbol_grab.out.value());
			} else if (base_element->getElementKind() == pst::ElementKind::ExprElement) {
				// @FIXME: change the way we check the condition, by comparing enum
				// values instead of strings. Make the enum stringifiable.
				return std::vector<SymID>{};
			} else {
				CORE_PANIC("Query symbols from scope of non-statement, non-codeblock and non-expr");
			}
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto output = getSymbols(ctx, key);

			// validate output:
			for (auto sym: output) {
				CORE_ASSERT(
					scope(sym) == key,
					base::strConcat(
						"Scope mismatch in QuerySymbolsInScope and QuerySymbolOfSTMT\n",
						" for symbol: ",
						name(sym),
						"\n\n"
						" considered scope : ",
						key.ref->related_pst_element.value()->elementType(),
						", ID: ",
						key.ref->related_pst_element.value()->getID().asInt(),
						"\n\n",
						" scope of symbol: ",
						scope(sym).ref->related_pst_element.value()->elementType(),
						", ID: ",
						scope(sym).ref->related_pst_element.value()->getID().asInt(),
						"\n"
					)
				);
			}
			return output;
		}

		static auto load(QKey key) -> LoadResult {
			if (const auto& cache = key.ref->symbols)
				return QResWithACD{ &cache->data, cache->acd };
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

	base::HashT KeyOf_LookupInScope::customPerfectHash() const {
		auto hash_1 = base::perfectHash(scope);
		auto hash_2 = std::hash<base::StrID>()(name);

		// @FIXME: this does not work:
		return (hash_1 * 143 + hash_2 * 7) * 2 + with_wildcards;
	}

	ScopeID queryRootScopeOfMainModuleFile(query::Context& ctx, frontend::ModuleID module) {
		auto main_source_file = ctx.query<frontend::QueryMainSourceFile>(module);
		auto main_source_pst  = ctx.query<frontend::QueryFilePST>(main_source_file);

		auto main_file_root_scope
			= ctx.query<QueryPrimaryCodeScopeFor>({ main_source_pst->getRootElement() });

		return main_file_root_scope;
	}

	void ScopeID::debugPrintScopeAndParents() {
		auto iter_scope = *this;

		while (true) {
			std::cerr << iter_scope.customPerfectHash() << "("
					  << (iter_scope.ref->related_pst_element.has_value()
			                  ? iter_scope.ref->related_pst_element.value()->elementType()
			                  : "ROOT")
					  << ")"
					  << " -> ";

			if (not parent(iter_scope).has_value()) break;
			iter_scope = parent(iter_scope).value();
		}
		std::cerr << "\n";
	}
}

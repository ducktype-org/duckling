#include "lookup.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/selector.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/import.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/using.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios_private/lookup/lookup_chain.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <diagnostic/placeholder.hpp>
#include <hashing/hash.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	base::Bit256 KeyOf_LookupInNamespaceOrModule::queryUnstablePerfectHash() const {
		return hashing::justHash<hashing::SHA256>(
			symbol.queryUnstablePerfectHash(),
			std::hash<base::StrID>()(name),
			static_cast<u64>(with_wildcards)
		);
	}

	base::Bit256 KeyOf_LookupInUsingOrImport::queryUnstablePerfectHash() const {
		return hashing::justHash<hashing::SHA256>(
			symbol.queryUnstablePerfectHash(),
			std::hash<base::StrID>()(name),
			static_cast<u64>(follow_wildcards)
		);
	}

	struct IMPLEMENT_QUERY(QueryLookupInNamespaceOrModule, query::QResult<LookupResult>) {
		static ScopeID getLinkedScope(Context& ctx, SymID id) {
			if (kind(id) == SymbolKind::Module) {
				auto module_id = getSymRef(id)->getData<defgen::Module>()->module_id;
				return queryRootScopeOfMainModuleFile(ctx, module_id);
			} else {
				CORE_ASSERT(kind(id) == SymbolKind::Namespace, "Invalid call");
				return queryBodyCodeScopeFor(ctx, getSymRef(id)->stmtCast(ctx).value());
			}
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key.symbol) == SymbolKind::Module or kind(key.symbol) == SymbolKind::Namespace,
				"Invalid call"
			);
			auto scope = getLinkedScope(ctx, key.symbol);
			UNPACK_QRESULT_CREF(
				auto result =, ctx.query<QueryLookupInScope>({ scope, key.name, key.with_wildcards })
			);
			return result;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInNamespaceOrModule);

	namespace {
		/**
		 * @brief The dotted prefix (`a.b` of `a.b.*` / `a.b as c`) of a single selector.
		 */
		std::vector<pst::AccessLocked<pst::IdentifierWrapper>> selectorPath(
			pst::Access<pst::Selector> selector
		) {
			std::vector<pst::AccessLocked<pst::IdentifierWrapper>> path;
			path.reserve(selector->numberOfNames());
			for (usize i = 0; i < selector->numberOfNames(); i++)
				path.push_back(selector->getNameByIndex(i));
			return path;
		}

		/**
		 * @brief Can this selector provide @p name, judging by the selector alone?
		 *
		 * The name a selector introduces is written in the selector itself: the `as` name for an
		 * alias, the last name of the path for a bare selector. A `.*` is the one tail that
		 * cannot be answered without looking inside what it points at, so it is taken as a yes
		 * whenever wildcards are being followed at all.
		 */
		query::QResult<bool> selectorValidForName(
			query::Context&            ctx,
			pst::Access<pst::Selector> selector,
			base::StrID                name,
			bool                       with_wildcards
		) {
			switch (selector->getTailKind()) {
			case pst::SelectorTail::As:
				return selector->getAsName().value().unlock(ctx)->unwrap() == name;
			case pst::SelectorTail::None: {
				if (selector->numberOfNames() == 0) return false;
				auto last_name = selector->getNameByIndex(selector->numberOfNames() - 1);
				return last_name.unlock(ctx)->unwrap() == name;
			}
			case pst::SelectorTail::Star:
				return with_wildcards;
			case pst::SelectorTail::Nested:
				ctx.log<dia::NotYetImplementedCodeError>(
					"Nested selector in usings and imports.", selector->getStablePosition()
				);
				return query::Failed();
			}
			CORE_PANIC("Unhandled selector tail kind");
		}

		SymID symOfModule(query::Context& ctx, frontend::ModuleID id) {
			return ctx.query<defgen::QueryGeneratedSymbol>({
				.name                  = frontend::moduleName(id),
				.generated_symbol_data = defgen::Module{ .module_id = id },
			});
		}
	}

	struct IMPLEMENT_QUERY(QueryLookupInUsingImport, query::QResult<LookupResult>) {
		/**
		 * @brief The selector list of the `using`/`import` statement the symbol was made from.
		 */
		static pst::AccessLocked<pst::SelectorList> selectorsOf(Context& ctx, SymID symbol) {
			auto element = getSymRef(symbol)->maybePstElement().value().unlock(ctx);
			if (auto using_stmt = element.dynamicCast<pst::Using>())
				return using_stmt.value()->getSelectors();
			return element.dynamicCast<pst::Import>().value()->getSelectors();
		}

		static query::QResult<LookupResult> lookupInSelector(
			Context&                   ctx,
			const QKey&                key,
			pst::Access<pst::Selector> selector,
			const LookupChainKey&      pointed_chain
		) {
			UNPACK_QRESULT_MOVE(auto sym_chain =, lookupChain(ctx, pointed_chain));
			CORE_ASSERT(
				not sym_chain.empty(),
				"It should be impossible to create chain that doesn't resolve to any symbol."
			);
			auto pointed = sym_chain.back();

			LookupResult result;
			if (selector->getTailKind() == pst::SelectorTail::Star) {
				UNPACK_QRESULT_CREF(
					result =, HInterface::ofSymbol(ctx, pointed).lookup(ctx, key.name)
				);
			} else {
				result.leaves.push_back(pointed);
			}

			return result;
		}

		/**
		 * @brief Lookup @p key.name through a single `using` selector.
		 *
		 * The path is an ordinary chain of names, looked up from the scope the `using` is in.
		 */
		static query::QResult<LookupResult> lookupInUsingSelector(
			Context& ctx, const QKey& key, pst::Access<pst::Selector> selector
		) {
			// The path of a `using` is resolved without wildcards: a wildcard cannot be part of
			// what another wildcard points at, and following them here would make resolving a
			// `using N.*;` ask the very scope it lives in for `N` again.
			LookupChainKey chain_key{ .names  = selectorPath(selector),
				                      .start  = scope(key.symbol),
				                      .params = { .with_wildcards = false } };

			UNPACK_QRESULT(auto result =, lookupInSelector(ctx, key, selector, chain_key));
			return result;
		}

		/**
		 * @brief Lookup @p key.name through a single `import` selector.
		 *
		 * The prefix of the selector is a module path, so the lookup continues in the root scope
		 * of that module.
		 */
		static query::QResult<LookupResult> lookupInImportSelector(
			Context& ctx, const QKey& key, pst::Access<pst::Selector> selector
		) {
			auto path_idents = selectorPath(selector);

			// Phase 1: follow the path through the module tree while it names modules.
			usize names_taken    = 0;
			auto  current_module = module(scope(key.symbol));
			for (; names_taken < path_idents.size(); names_taken++) {
				auto lookup_name = path_idents.at(names_taken).unlock(ctx)->unwrap();
				auto maybe_imported_module
					= frontend::getRelativeModule(ctx, current_module, { lookup_name });
				if (not maybe_imported_module) {
					// The first name has to be a module, the rest may already be its contents.
					if (names_taken == 0) {
						ctx.log<dia::PlaceholderError>(
							"Module not found.", selector->getStablePosition()
						);
						return query::Failed();
					}
					break;
				}
				current_module = maybe_imported_module.value();
			}

			// Phase 2: what is left of the path names items inside the module found above.
			// Nothing left means the selector points at the module itself, which is an empty chain.
			LookupChainKey chain_key{
				.names = std::vector(
					path_idents.begin() + base::safeIntConv<int>(names_taken), path_idents.end()
				),
				.start  = symOfModule(ctx, current_module),
				.params = { .with_wildcards = false },
			};

			UNPACK_QRESULT(auto result =, lookupInSelector(ctx, key, selector, chain_key));
			return result;
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			const auto symbol_kind = kind(key.symbol);
			CORE_ASSERT(
				symbol_kind == SymbolKind::Using or symbol_kind == SymbolKind::Import, "Invalid call"
			);

			auto selectors = selectorsOf(ctx, key.symbol).unlock(ctx);

			LookupResult result{};
			bool         failed = false;

			for (const auto& selector_access: *selectors) {
				auto selector = selector_access.unlock(ctx);

				// This is needed to avoid query cycle in non-wildcard usings and imports.
				auto matches_result
					= selectorValidForName(ctx, selector, key.name, key.follow_wildcards);
				if (matches_result.hasFailed()) {
					failed = true;
					continue;
				}
				if (not matches_result.valueOrPanic()) continue;

				auto selector_result = symbol_kind == SymbolKind::Using
				                         ? lookupInUsingSelector(ctx, key, selector)
				                         : lookupInImportSelector(ctx, key, selector);
				if (selector_result.hasFailed()) {
					failed = true;
					continue;
				}

				result.merge(std::move(selector_result.valueOrPanic()));
			}

			if (failed) return query::Failed();

			return result;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInUsingImport);
}

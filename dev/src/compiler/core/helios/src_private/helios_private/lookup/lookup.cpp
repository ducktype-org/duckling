#include "lookup.hpp"

#include "frontend/module_tree/queries.hpp"
#include "frontend/pst_parser/elements/hierarchy/not_statements/selector.hpp"
#include "frontend/pst_parser/elements/hierarchy/statements/import.hpp"
#include "frontend/pst_parser/elements/hierarchy/statements/using.hpp"
#include "helios/symbols/symbol_kind.hpp"
#include "helios_private/lookup/lookup_chain.hpp"
#include "helios_private/scopes/scopes.hpp"
#include "helios_private/symbols/symbol_data.hpp"
#include "helios_private/symbols/symbols.hpp"

#include "diagnostic/placeholder.hpp"
#include "hashing/hash.hpp"
#include "query_framework/query_result.hpp"
#include "query_framework/standard_query/query_impl.hpp"

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
				return scope(id);
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
		 * @brief The dotted prefix of a single selector, as plain names.
		 */
		std::vector<base::StrID> selectorPathNames(
			query::Context& ctx, pst::Access<pst::Selector> selector
		) {
			std::vector<base::StrID> path;
			path.reserve(selector->numberOfNames());
			for (usize i = 0; i < selector->numberOfNames(); i++)
				path.push_back(selector->getNameByIndex(i).unlock(ctx)->unwrap());
			return path;
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
			pst::Access<pst::Selector> tail_of_selector,
			const LookupChainKey&      lookup_chain_key
		) {
			UNPACK_QRESULT_MOVE(auto pointed =, lookupChain(ctx, lookup_chain_key));

			LookupResult result;
			switch (tail_of_selector->getTailKind()) {
			case pst::SelectorTail::None:
				if (name(pointed.back()) == key.name) result.leaves.push_back(pointed.back());
				break;
			case pst::SelectorTail::As: {
				auto name = tail_of_selector->getAsName().value().unlock(ctx)->unwrap();
				if (name == key.name) result.leaves.push_back(pointed.back());
				break;
			}
			case pst::SelectorTail::Star: {
				// `using a.*;` / `import a.*;` only contribute names when wildcards are followed.
				if (not key.follow_wildcards) break;
				auto sym_interface = getSymbolInterface(ctx, pointed.back());
				UNPACK_QRESULT_CREF(result =, sym_interface.lookup(ctx, key.name));
				break;
			}
			case pst::SelectorTail::Nested: {
				ctx.log<dia::NotYetImplementedCodeError>(
					"Nested selector in usings and imports.", tail_of_selector->getStablePosition()
				);
			}
			}

			return result;
		}

		static query::QResult<LookupResult> lookupInUsingSelector(
			Context& ctx, const QKey& key, pst::Access<pst::Selector> selector
		) {
			auto           begin_scope = scope(key.symbol);
			auto           names       = selectorPath(selector);
			LookupChainKey chain_key{ .names  = std::move(names),
				                      .start  = begin_scope,
				                      .params = { .with_wildcards = key.follow_wildcards } };
			UNPACK_QRESULT(auto result =, lookupInSelector(ctx, key, selector, chain_key));
			return result;
		}

		/**
		 * @brief Lookup @p name through a single `import` selector.
		 *
		 * The prefix of the selector is a module path, so the lookup continues in the root scope
		 * of that module.
		 */
		static query::QResult<LookupResult> lookupInImportSelector(
			Context& ctx, const QKey& key, pst::Access<pst::Selector> selector
		) {
			auto path_idents = selectorPath(selector);

			// Phase 1: try to find the modules from the chain until we don't.
			int  i{ 0 };
			auto current_module = module(scope(key.symbol));
			for (; i < path_idents.size(); i++) {
				auto lookup_name = path_idents.at(0).unlock(ctx)->unwrap();
				auto maybe_imported_module
					= frontend::getRelativeModule(ctx, current_module, { lookup_name });
				if (not maybe_imported_module) {
					// If this is the first name in the chain, we throw an error.
					if (i == 0) {
						ctx.log<dia::PlaceholderError>(
							"Import didn't find any modules.", selector->getStablePosition()
						);
						return query::Failed();
					}
					// If not, we continue with the second phase.
					break;
				}
				current_module = maybe_imported_module.value();
			}

			// Phase 2: we look at the items in the imported module with the remaining chain..
			std::vector    rest_of_the_path(path_idents.begin() + i, path_idents.end());
			LookupChainKey chain_key{
				.names  = std::move(rest_of_the_path),
				.start  = symOfModule(ctx, current_module),
				.params = { .with_wildcards = key.follow_wildcards },
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
				auto selector        = selector_access.unlock(ctx);
				auto selector_result = symbol_kind == SymbolKind::Using
				                         ? lookupInUsingSelector(ctx, key, selector)
				                         : lookupInImportSelector(ctx, key, selector);
				if (selector_result.hasFailed()) {
					failed = true;
					continue;
				}

				result.merge(std::move(selector_result.valueOrThrow()));
			}

			if (failed) return query::Failed();

			return result;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInUsingImport);
}

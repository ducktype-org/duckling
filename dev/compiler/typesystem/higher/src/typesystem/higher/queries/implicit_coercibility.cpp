#include "implicit_coercibility.hpp"

#include <set>

#include <query_framework/query_impl.hpp>

namespace tsh {
	struct IMPLEMENT_QUERY(QueryImplicitCoercibilityOnAbstractType, bool) {
		static auto provide(Context& context, const QKey key) -> PResult {
			return key.source == key.target
			    || getImplicitConversionsFrom(key.source, context).contains(key.target)
			    || getImplicitConstructorsOf(key.target, context).contains(key.source)
			    || key.source.isImplicitlyCoercible(key.target, context);
		}

		QUERY_AUTO_CACHE_COPY

	private:
		static std::set<AbstractType> getImplicitConversionsFrom(
			[[maybe_unused]] AbstractType source, [[maybe_unused]] Context& context
		) {
			// @TODO: Add implementation when query for extracting implicit
			// conversion operators for types appears.
			return {};
		}

		static std::set<AbstractType> getImplicitConstructorsOf(
			[[maybe_unused]] AbstractType target, [[maybe_unused]] Context& context
		) {
			// @TODO: Add implementation when query for extracting implicit
			// single-argument constructors for types appears.
			return {};
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryImplicitCoercibilityOnAbstractType);

	struct IMPLEMENT_QUERY(QueryImplicitCoercibilityOnSymbolType, bool) {
		static auto provide(Context& context, const QKey& key) -> PResult {
			// @TODO: #584
			return context.query<QueryImplicitCoercibilityOnAbstractType>({
					   key.source.getType(),
					   key.target.getType(),
				   })
			   and (key.source.getMutability() == Mutability::Mutable
			        or key.target.getMutability() == Mutability::Immutable);
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryImplicitCoercibilityOnSymbolType);

	struct IMPLEMENT_QUERY(QueryImplicitCoercibilityOnExpressionType, bool) {
		static auto provide(Context& context, const QKey& key) -> PResult {
			bool symbol_type_coercibility = context.query<QueryImplicitCoercibilityOnSymbolType>(
				{ key.source.getSymbolType(), key.target.getSymbolType() }
			);
			bool vc_coercibility
				= key.source.getValueCategory().contains(key.target.getValueCategory());
			return symbol_type_coercibility && vc_coercibility;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryImplicitCoercibilityOnExpressionType);
}

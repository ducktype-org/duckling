#include "implicit_coercibility.hpp"

#include <query_framework/standard_query/query_impl.hpp>

#include <set>

namespace compiler::tsh {
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
			const auto from_ref_kind = key.source.getRefKind();
			const auto to_ref_kind   = key.target.getRefKind();

			/*
			 * The current coercion logic regarding reference kinds is:
			 * 						  FROM
			 * 			    | Direct | Ref | Box
			 *	 	Direct 	|  Yes	 | Yes | Yes
			 * TO 	Ref		|   No   | Yes | No
			 *	 	Box		|   No   | No  | Yes
			 */
			if (from_ref_kind == ReferenceKind::Direct && to_ref_kind == ReferenceKind::Ref)
				return false;

			if (from_ref_kind == ReferenceKind::Box && to_ref_kind == ReferenceKind::Ref)
				return false;

			if (to_ref_kind == ReferenceKind::Box && from_ref_kind != ReferenceKind::Box)
				return false;

			// For pointer-like symbol types (ex. ref/box) the element types must match exactly.
			if (to_ref_kind != ReferenceKind::Direct)
				return key.source.getType() == key.target.getType();

			// @TODO: #584
			return context.query<QueryImplicitCoercibilityOnAbstractType>({
				key.source.getType(),
				key.target.getType(),
			});
			// @TODO: #1488 readd/rethink mutability handling here
			//    and (key.source.getMutability() == Mutability::Mutable
			//         or key.target.getMutability() == Mutability::Immutable);
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

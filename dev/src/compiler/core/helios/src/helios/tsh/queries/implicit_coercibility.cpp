#include "implicit_coercibility.hpp"

#include <helios/tsh/coercions/reference_coercion.hpp>
#include <helios/tsh/types.hpp>

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

			if (!referenceCoercionRule(from_ref_kind, to_ref_kind).isLegal()) return false;

			// For pointer-like symbol types (ex. ref/box) the element types must match exactly.
			if (to_ref_kind != ReferenceKind::Direct)
				return key.source.getType() == key.target.getType();

			// A value coerces into a variant only when its type is exactly equal to one of the
			// variant's alternatives (no chained coercions).
			if (key.target.getType().getKind() == Kind::Variant
			    && key.source.getType().getKind() != Kind::Variant) {
				const VariantAbstractType target_variant = key.target.getType();
				for (const auto& alternative: target_variant.getUnderlyingTypes())
					if (referenceCoercionRule(from_ref_kind, alternative.getRefKind()).isLegal()
					    && alternative.getType() == key.source.getType())
						return true;
			}

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

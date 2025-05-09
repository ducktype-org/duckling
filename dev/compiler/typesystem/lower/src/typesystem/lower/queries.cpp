#include "queries.hpp"

#include <query_framework/query_impl.hpp>

namespace tsl {
	struct IMPLEMENT_QUERY(QueryAbstractTypeLayout, TypeLayout) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			using enum tsh::Kind;
			switch (key.getKind()) {
			case Unit:
				return EmptyTypeLayout(key);
			case Byte:
				return IntegralTypeLayout(tsh::ByteAbstractType(key));
			case Bool:
				return IntegralTypeLayout(tsh::BoolAbstractType(key));
			case Char:
				return IntegralTypeLayout(tsh::CharAbstractType(key));
			case Integral:
				return IntegralTypeLayout(tsh::IntegralAbstractType(key));
			case Float:
				return FloatTypeLayout(key);
			case RawPointer:
				return PointerTypeLayout(key);
			case Pointer:
				return PointerTypeLayout(key, ctx);
			case String:
				return StringTypeLayout(key);
			case Function:
				return FunctionalTypeLayout(key);
			case Variant:
				return VariantTypeLayout(key, ctx);
			case Tuple:
				return TupleTypeLayout(tsh::TupleAbstractType(key), ctx);
			case Class:
				return ClassTypeLayout(tsh::ClassAbstractType(key), ctx);
			default:
				CORE_PANIC("Unsupported source type in QueryAbstractTypeLayout.");
			}
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryAbstractTypeLayout)

	struct IMPLEMENT_QUERY(QuerySymbolTypeLayout, TypeLayout) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			if (key.getRefKind() == tsh::ReferenceKind::Direct)
				return ctx.query<QueryAbstractTypeLayout>(key.getType());
			return PointerTypeLayout(key, ctx);
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolTypeLayout)
}

#include "queries.hpp"

#include <query_framework/standard_query/query_impl.hpp>

#include <utility>

namespace compiler::tsl {
	struct IMPLEMENT_QUERY(QueryAbstractTypeLayout, query::QResult<TypeLayout>) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			using enum tsh::Kind;
			switch (key.getKind()) {
			case Unit:
				return EmptyTypeLayout(key, ctx);
			case Meta:
				return MetaTypeLayout(tsh::MetaAbstractType(key), ctx);
			case Byte:
				return IntegralTypeLayout(tsh::ByteAbstractType(key), ctx);
			case Bool:
				return IntegralTypeLayout(tsh::BoolAbstractType(key), ctx);
			case Char:
				return IntegralTypeLayout(tsh::CharAbstractType(key), ctx);
			case Integral:
				return IntegralTypeLayout(tsh::IntegralAbstractType(key), ctx);
			case Float:
				return FloatTypeLayout(key, ctx);
			case RawPointer:
				return PointerTypeLayout(tsh::RawPointerAbstractType(key), ctx);
			case Pointer:
				return PointerTypeLayout(tsh::PointerAbstractType(key), ctx);
			case ManyPointer:
				return PointerTypeLayout(tsh::ManyPointerAbstractType(key), ctx);
			case CPointer:
				return PointerTypeLayout(tsh::CPointerAbstractType(key), ctx);
			case Slice:
				return ClassTypeLayout(tsh::SliceAbstractType(key), ctx);
			case String:
				return StringTypeLayout(key, ctx);
			case Function:
				return FunctionalTypeLayout(key, ctx);
			case DynamicArray:
				return ClassTypeLayout(tsh::DynamicArrayAbstractType(key), ctx);
			case StaticArray:
				return StaticArrayTypeLayout(tsh::StaticArrayAbstractType(key), ctx);
			case Variant:
				return VariantTypeLayout(key, ctx);
			case Tuple:
				return ClassTypeLayout(tsh::TupleAbstractType(key), ctx);
			case Class:
				return ClassTypeLayout(tsh::ClassAbstractType(key), ctx);
			default:
				CORE_PANIC("Unsupported source type in QueryAbstractTypeLayout.");
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryAbstractTypeLayout)

	struct IMPLEMENT_QUERY(QuerySymbolTypeLayout, query::QResult<TypeLayout>) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			if (key.getRefKind() == tsh::ReferenceKind::Direct) {
				auto copy = ctx.query<QueryAbstractTypeLayout>(key.getType())->valueOrThrow();
				return std::move(copy);
			}
			return PointerTypeLayout(key, ctx);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySymbolTypeLayout)
}

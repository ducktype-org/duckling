#include "queries.hpp"

#include <query_framework/query_impl.hpp>

namespace tsl {
	struct IMPLEMENT_QUERY(QueryTypeLayout, TypeLayout) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			using enum tsh::Kind;
			switch (key.getKind()) {
			case Unit:
				return EmptyTypeLayout(key);
			case Byte:
				return IntegralTypeLayout(tsh::ByteInfo(key));
			case Bool:
				return IntegralTypeLayout(tsh::BoolInfo(key));
			case Char:
				return IntegralTypeLayout(tsh::CharInfo(key));
			case Integral:
				return IntegralTypeLayout(tsh::IntegralInfo(key));
			case Float:
				return FloatTypeLayout(key);
			case RawPointer:
				return PointerTypeLayout(key);
			case Pointer:
				return PointerTypeLayout(key, ctx);
			case Function:
				return FunctionalTypeLayout(key);
			case Variant:
				return VariantTypeLayout(key, ctx);
			case Tuple:
				return TupleTypeLayout(tsh::TupleInfo(key), ctx);
			case Class:
				return ClassTypeLayout(tsh::ClassInfo(key), ctx);
			default:
				CORE_PANIC("Unsupported source type in QueryTypeLayout.");
			}
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeLayout)
}

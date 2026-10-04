#include "length_methods.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	using namespace code::shorthands;

	namespace {
		Box<code::Expr> buildSliceTypeLengthExpr(
			query::Context& ctx, const tsh::SliceAbstractType& slice_type, const SymID source_symbol
		) {
			const Shorthand s{ ctx };
			const auto      slice_fields = ctx.query<QuerySliceTypeData>(slice_type);
			return s.access(s.ident(source_symbol), slice_fields->len);
		}

		Box<code::Expr> buildStaticArrayTypeLengthExpr(
			query::Context& ctx, const tsh::StaticArrayAbstractType& static_array_type
		) {
			const Shorthand s{ ctx };
			return s.litNum(
				numeric_value::NumericValue::createOfType(
					tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed),
					static_array_type.getSize()
				)
					.value()  // This will always succeed.
			);
		}

	}

	SymID lengthMethodForType(query::Context& ctx, const tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>({ .name                  = base::StrID("length"),
		                                         .generated_symbol_data = Method{
													 .owner_type = type,
													 .kind       = Method::Kind::LengthMethod,
												 } });
	}

	struct IMPLEMENT_QUERY(QueryLengthMethod, query::QResult<HOUTFunction>) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			const Shorthand s{ ctx };

			auto& decl = ctx.query<QueryDeclOfFun>(lengthMethodForType(ctx, key))->valueOrThrow();

			SymID source_symbol = decl.parameters.at(0).helios_symbol;

			Box<code::Expr> length_expr = [&]() {
				switch (key.getKind()) {
				case tsh::Kind::Slice:
					return buildSliceTypeLengthExpr(
						ctx, key.as<tsh::SliceAbstractType>(), source_symbol
					);
				case tsh::Kind::StaticArray:
					return buildStaticArrayTypeLengthExpr(
						ctx, key.as<tsh::StaticArrayAbstractType>()
					);
				default:
					CORE_UNREACHABLE();
				}
			}();

			auto body = StmtPack{ s.ret(std::move(length_expr)) }.toCodeBlock();

			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(std::move(body))
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLengthMethod);
}

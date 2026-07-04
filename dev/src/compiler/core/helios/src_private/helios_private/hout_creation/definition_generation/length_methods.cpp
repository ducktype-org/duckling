#include "length_methods.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	namespace {
		Box<code::Expr> buildSliceTypeLengthExpr(
			query::Context& ctx, const tsh::SliceAbstractType& slice_type, SymID source_symbol
		) {
			auto slice_fields = ctx.query<QuerySliceTypeData>(slice_type);
			return makeBox<code::AccessExpr>(
				ctx,
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), source_symbol),
				slice_fields->len
			);
		}

		Box<code::Expr> buildStaticArrayTypeLengthExpr(
			query::Context& ctx, const tsh::StaticArrayAbstractType& static_array_type
		) {
			return makeBox<code::LiteralNumericExpr>(
				ctx,
				code::generatedOrigin(),
				numeric_value::NumericValue::createOfType(
					tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned),
					static_array_type.getSize()
				)
					.value()  // This will always succeed.
			);
		}

		Box<code::Expr> buildDynamicArrayTypeLengthExpr(
			query::Context&                      ctx,
			const tsh::DynamicArrayAbstractType& dynamic_array_type,
			SymID                                source_symbol
		) {
			auto dyn_fields = ctx.query<QueryDynamicArrayTypeData>(dynamic_array_type);
			return makeBox<code::AccessExpr>(
				ctx,
				code::generatedOrigin(),

				makeBox<code::DerefExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), source_symbol)
				),
				dyn_fields->len
			);
		}
	}

	SymID lengthMethodForType(query::Context& ctx, tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>({ .name                  = base::StrID("length"),
		                                         .generated_symbol_data = GeneratedSymbolData{
													 GeneratedSymbolData::LengthMethod{ type } } });
	}

	struct IMPLEMENT_QUERY(QueryLengthMethod, query::QResult<HOUTFunction>) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			auto& decl = ctx.query<QueryDeclOfFun>(lengthMethodForType(ctx, key))->valueOrThrow();

			SymID                        source_symbol = decl.parameters.at(0).helios_symbol;
			std::vector<Box<code::Stmt>> body{};

			Box<code::Expr> length_expr = [&]() {
				switch (key.getKind()) {
				case tsh::Kind::Slice:
					return buildSliceTypeLengthExpr(
						ctx, key.as<tsh::SliceAbstractType>(), source_symbol
					);
				case tsh::Kind::DynamicArray:
					return buildDynamicArrayTypeLengthExpr(
						ctx, key.as<tsh::DynamicArrayAbstractType>(), source_symbol
					);
				case tsh::Kind::StaticArray:
					return buildStaticArrayTypeLengthExpr(
						ctx, key.as<tsh::StaticArrayAbstractType>()
					);
				default:
					CORE_UNREACHABLE();
				}
			}();

			body.emplace_back(
				makeBox<code::ReturnStmt>(code::generatedOrigin(), std::move(length_expr))
			);

			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLengthMethod);
}

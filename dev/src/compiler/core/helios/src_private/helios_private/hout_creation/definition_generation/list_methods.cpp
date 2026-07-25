#include "list_methods.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	using namespace code::shorthands;

	SymID pushMethodForType(query::Context& ctx, tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>(
			{ .name                  = base::StrID("push"),
		      .generated_symbol_data = Method{ .owner_type = type, .kind = Method::Kind::Push } }
		);
	}

	SymID popMethodForType(query::Context& ctx, tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>(
			{ .name                  = base::StrID("pop"),
		      .generated_symbol_data = Method{ .owner_type = type, .kind = Method::Kind::Pop } }
		);
	}

	struct IMPLEMENT_QUERY(QueryPushMethod, query::QResult<HOUTFunction>) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			auto& decl = ctx.query<QueryDeclOfFun>(pushMethodForType(ctx, key))->valueOrThrow();

			const SymID self_symbol    = decl.parameters.at(0).helios_symbol;
			const SymID element_symbol = decl.parameters.at(1).helios_symbol;

			const Shorthand s{ ctx };

			auto body
				= StmtPack{ s.expr(s.listPush(s.deref(s.ident(self_symbol)), s.ident(element_symbol))
				            ) }
			          .toCodeBlock();

			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(std::move(body))
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPushMethod);

	struct IMPLEMENT_QUERY(QueryPopMethod, query::QResult<HOUTFunction>) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			auto& decl = ctx.query<QueryDeclOfFun>(popMethodForType(ctx, key))->valueOrThrow();

			const SymID self_symbol  = decl.parameters.at(0).helios_symbol;
			const SymID count_symbol = decl.parameters.at(1).helios_symbol;

			const Shorthand s{ ctx };

			auto body
				= StmtPack{ s.expr(s.listPop(s.deref(s.ident(self_symbol)), s.ident(count_symbol))) }
			          .toCodeBlock();

			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(std::move(body))
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPopMethod);
}

#include "tuple_constructor.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	struct IMPLEMENT_QUERY(QueryTuplePackConstructor, query::QResult<HOUTFunction>) {
		static auto provide(Context& ctx, const QKey tuple_type) -> PResult {
			auto tuple_interface = tuple_type.getInterface(ctx);
			// Preamble, get some basic data.
			using defgen::Constructor;
			using std::ranges::to;
			using std::views::transform;

			// Construct the constructor's type.
			// @TODO: #1328 Properly handle value categories in class constructors.
			const std::vector<tsh::InterfaceElement> fields
				= tuple_interface->getFieldsView() | to<std::vector>();
			const u64 num_fields = fields.size();

			// Prepare the ctor symbol and declaration.
			const SymID ctor_symbol = ctx.query<QueryGeneratedSymbol>({
				.name
				= ctx.query<mangler::QueryMangledType>(tsh::SymbolType<>::withDefaults(tuple_type))
			          ->valueOrThrow(),
				.generated_symbol_data
				= Constructor{ .type = tuple_type, .kind = Constructor::Kind::Implicit },
			});

			const auto& ctor_decl = ctx.query<QueryDeclOfFun>(ctor_symbol)->valueOrThrow();

			// Prepare the body of the constructor.
			using namespace code::shorthands;
			const Shorthand s{ ctx };

			// Build the tuple value directly from its parameters, one per field in declaration
			// order, and return it. Each parameter owns its value and is consumed here, so it is
			// moved into the aggregate rather than copied.
			std::vector<Box<code::Expr>> field_values;
			field_values.reserve(num_fields);
			for (usize i = 0; i < num_fields; i++)
				field_values.emplace_back(s.move(s.ident(ctor_decl.parameters.at(i).helios_symbol)));

			std::vector<Box<code::Stmt>> body{};
			body.emplace_back(
				s.ret(s.createAggregate(ctor_decl.return_type.getType(), std::move(field_values)))
			);

			// Finally, create the HOUTFunction object.
			return HOUTFunction(
				code::generatedOrigin(),
				&ctor_decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTuplePackConstructor);
}

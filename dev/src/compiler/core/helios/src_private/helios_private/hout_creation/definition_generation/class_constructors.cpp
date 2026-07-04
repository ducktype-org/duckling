#include "class_constructors.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/stable_container.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::defgen {
	struct IMPLEMENT_QUERY(QueryImplicitClassConstructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey class_type) {
			// Preamble, get some basic data.
			auto class_interface = class_type.getInterface(ctx);

			using defgen::Constructor;
			using defgen::GeneratedConstructorKind;
			using Variable = GeneratedFunctionVariable;
			using std::ranges::to;
			using std::views::transform;

			// Construct the constructor's type.
			// @TODO: #1328 Properly handle value categories in class constructors.
			const std::vector<tsh::InterfaceElement> fields
				= class_interface->getFieldsView() | to<std::vector>();
			const u64 num_fields = fields.size();

			// Prepare the ctor symbol and declaration.
			const SymID ctor_symbol = ctx.query<QueryGeneratedSymbol>({
				.name = name(class_type.getSymbol()),
				.generated_symbol_data
				= Constructor{ .type = class_type, .kind = GeneratedConstructorKind::Implicit },
			});

			const auto& ctor_decl = ctx.query<QueryDeclOfFun>(ctor_symbol)->valueOrThrow();

			// Prepare the body of the constructor.
			std::vector<Box<code::Stmt>> body{};

			// - One declaration, one assignment per field, one return.
			body.reserve(1 + num_fields + 1);

			// - Declare result variable.
			const auto result_symbol_type = ctor_decl.return_type;
			// @TODO: #2307 Classes with a field named `__result` don't work.
			const SymID result_symbol = ctx.query<QueryGeneratedSymbol>({
				.name                  = base::StrID("__result"),
				.generated_symbol_data = Variable{ ctor_symbol, 0, result_symbol_type },
			});
			body.emplace_back(makeBox<code::VariableStmt>(code::VariableStmt(
				code::generatedOrigin(),
				makeBox<code::DefaultValueExpr>(
					ctx, code::generatedOrigin(), result_symbol_type.getType()
				),
				result_symbol_type,
				result_symbol
			)));

			// - Assign each field from the corresponding parameter.
			for (usize i = 0; i < num_fields; i++) {
				body.emplace_back(makeBox<code::AssignmentStmt>(
					code::generatedOrigin(),
					makeBox<code::AccessExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_symbol),
						fields.at(i).getSymbol()
					),
					makeBox<code::IdentifierExpr>(
						ctx, code::generatedOrigin(), ctor_decl.parameters.at(i).helios_symbol
					)
				));
			}

			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_symbol)
			));

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

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryImplicitClassConstructor);
}

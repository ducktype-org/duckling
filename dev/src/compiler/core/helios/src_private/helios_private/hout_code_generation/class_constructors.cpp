#include "class_constructors.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/internal/queries.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <base/stable_container.hpp>

#include <query_framework/query_impl.hpp>

namespace compiler::helios::houtgen {
	struct IMPLEMENT_QUERY(QueryImplicitClassConstructor, HOUTFunction) {
		static PResult provide(Context& ctx, const QKey class_type) {
			// Preamble, get some basic data.
			const SymID               class_symbol    = class_type.getSymbol();
			const tsh::TypeInterface& class_interface = class_type.getInterface(ctx);

			using ImplicitConstructor = GeneratedSymbolData::ImplicitConstructor;
			using Parameter           = GeneratedSymbolData::Parameter;
			using Variable            = GeneratedSymbolData::Variable;
			using std::ranges::to;
			using std::views::transform;

			// Construct the constructor's type.
			// @TODO: #1328 Properly handle value categories in class constructors.
			std::vector<tsh::InterfaceElement> fields
				= class_interface.getFieldsView() | to<std::vector>();
			const u64 num_fields = fields.size();

			// Prepare the necessary symbols (of the constructor and its parameters).
			const SymID ctor_symbol = ctx.query<QueryGeneratedSymbol>({
				.name                  = name(class_type.getSymbol()),
				.generated_symbol_data = GeneratedSymbolData{ ImplicitConstructor{ class_symbol } },
			});

			// - The parameter symbols.
			u64                          argument_index = 0;
			std::vector<code::Parameter> parameters;
			parameters.reserve(num_fields);

			for (auto field: fields) {
				const SymID argument_symbol = ctx.query<QueryGeneratedSymbol>(
					{ .name = base::StrID(name(field.getSymbol())),
				      .generated_symbol_data
				      = GeneratedSymbolData{ Parameter{ ctor_symbol, argument_index } } }
				);
				// @TODO: #1328 Properly handle value categories in class constructors.
				parameters.emplace_back(
					name(argument_symbol), field.getType(ctx), std::nullopt, argument_symbol
				);
				argument_index++;
			}

			// - The result variable symbol
			const auto result_symbol_type = tsh::SymbolType<>{
				class_type,
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
			const SymID result_symbol = ctx.query<QueryGeneratedSymbol>({
				.name = base::StrID("result"),
				.generated_symbol_data
				= GeneratedSymbolData{ Variable{ ctor_symbol, 0, result_symbol_type } },
			});

			// Prepare the body of the constructor.
			std::vector<Box<code::Stmt>> body{};

			// - One declarations, one assignment per field, one return.
			body.reserve(1 + num_fields + 1);

			// - Declare result variable.
			body.emplace_back(
				makeBox<code::VariableStmt>(std::nullopt, result_symbol_type, result_symbol)
			);

			// - Assign each field from the corresponding parameter.
			for (usize i = 0; i < num_fields; i++) {
				body.emplace_back(makeBox<code::AssignmentStmt>(
					makeBox<code::AccessExpr>(
						ctx,
						makeBox<code::IdentifierExpr>(ctx, result_symbol),
						name(fields.at(i).getSymbol())
					),
					makeBox<code::IdentifierExpr>(ctx, parameters.at(i).helios_symbol)
				));
			}

			body.emplace_back(
				makeBox<code::ReturnStmt>(makeBox<code::IdentifierExpr>(ctx, result_symbol))
			);

			// Finally, create the HOUTFunction object.
			return HOUTFunction{
				HOUTFunctionDeclaration{
					ctor_symbol,
					result_symbol_type,
					std::make_shared<std::vector<code::Parameter>>(std::move(parameters)),
				},
				std::make_shared<const code::CodeBlock>(code::CodeBlock{ .statements
				                                                         = std::move(body) }),
			};
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryImplicitClassConstructor);
}

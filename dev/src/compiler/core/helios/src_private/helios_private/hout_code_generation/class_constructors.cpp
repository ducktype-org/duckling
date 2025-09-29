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
			using Variable            = GeneratedSymbolData::Variable;
			using std::ranges::to;
			using std::views::transform;

			// Construct the constructor's type.
			// @TODO: #1328 Properly handle value categories in class constructors.
			std::vector<tsh::InterfaceElement> fields
				= class_interface.getFieldsView() | to<std::vector>();
			const u64 num_fields = fields.size();

			const tsh::SymbolType<> self_type{
				class_type,
				tsh::ReferenceKind::Ref,
				tsh::Mutability::Mutable,
			};

			// Prepare the necessary symbols (of the constructor and its parameters).
			const SymID ctor_symbol = ctx.query<QueryGeneratedSymbol>({
				.name                  = name(class_type.getSymbol()),
				.generated_symbol_data = { ImplicitConstructor{ class_symbol } },
			});

			// - The self parameter symbol.
			u64             argument_index = 0;
			const SymID     self_symbol    = ctx.query<QueryGeneratedSymbol>({
					   .name                  = base::StrID("self"),
					   .generated_symbol_data = { Variable{ ctor_symbol, argument_index } },
            });
			code::Parameter first_parameter{
				.name          = base::StrID(""),
				.type          = self_type,
				.initial_value = std::nullopt,
				.helios_symbol = self_symbol,
			};

			// - The other parameter symbols.
			auto field_parameters
				= fields | transform([&](const tsh::InterfaceElement& field) {
					  argument_index++;
					  const SymID argument_symbol = ctx.query<QueryGeneratedSymbol>(
						  { .name                  = base::StrID(name(field.getSymbol())),
				            .generated_symbol_data = { Variable{ ctor_symbol, argument_index } } }
					  );
					  // @TODO: #1328 Properly handle value categories in class constructors.
					  return code::Parameter{
						  .name          = name(argument_symbol),
						  .type          = field.getType(ctx),
						  .initial_value = std::nullopt,
						  .helios_symbol = argument_symbol,
					  };
				  });

			// - All parameter symbols in one vector.
			std::vector<code::Parameter> parameters{};
			parameters.emplace_back(
				first_parameter.name,
				first_parameter.type,
				std::nullopt,
				first_parameter.helios_symbol
			);
			for (const code::Parameter& param: field_parameters)
				parameters.emplace_back(param.name, param.type, std::nullopt, param.helios_symbol);

			// Prepare the body of the constructor.
			std::vector<Box<code::Stmt>> body{};
			// - One assignment per field + return.
			body.reserve(num_fields + 1);

			std::cerr << "Doing...\n";
			for (usize i = 0; i < num_fields; i++) {
				std::cerr << "Doing " << i << "\n";
				body.emplace_back(makeBox<code::AssignmentStmt>(
					makeBox<code::AccessExpr>(
						ctx,
						makeBox<code::IdentifierExpr>(ctx, self_symbol),
						name(fields.at(i).getSymbol())
					),
					makeBox<code::IdentifierExpr>(ctx, parameters.at(i + 1).helios_symbol)
				));
			}
			std::cerr << "Done\n";

			body.emplace_back(makeBox<code::VoidReturnStmt>());

			// Finally, create the HOUTFunction object.
			return HOUTFunction{
				HOUTFunctionDeclaration{
					ctor_symbol,
					tsh::SymbolType<>(
						ctx.query<tsh::QueryUnitType>({}),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable
					),
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

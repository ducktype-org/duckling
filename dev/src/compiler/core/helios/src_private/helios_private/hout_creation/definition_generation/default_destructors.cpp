#include "default_destructors.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	struct IMPLEMENT_QUERY(QueryDefaultDestructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey owner_type) {
			const auto  dtor_sym  = ctx.query<QueryGeneratedSymbol>({
                base::StrID("__destruct"),
                GeneratedSymbolData{ GeneratedSymbolData::DefaultDestructor{ owner_type } },
            });
			const auto& dtor_decl = ctx.query<QueryDeclOfFun>(dtor_sym)->valueOrThrow();

			std::vector<Box<code::Stmt>> body{};
			Box<code::Expr>              dereffed_self = makeBox<code::DerefExpr>(
                ctx,
                code::generatedOrigin(),
                makeBox<code::IdentifierExpr>(
                    ctx, code::generatedOrigin(), dtor_decl.parameters.at(0).helios_symbol
                )
            );

			switch (owner_type.getKind()) {
			case tsh::Kind::Class: {
				const auto class_type      = owner_type.as<tsh::ClassAbstractType>();
				auto       class_interface = class_type.getInterface(ctx);

				const std::vector<tsh::InterfaceElement> fields
					= class_interface->getFieldsView() | std::ranges::to<std::vector>();

				for (const auto& field: std::views::reverse(fields)) {
					const auto field_type = field.getType(ctx);

					if (field_type.getType().getKind() == tsh::Kind::Class) {
						const SymID field_dtor_sym = ctx.query<QueryGeneratedSymbol>({
							.name = base::StrID("__destruct"),
							.generated_symbol_data
							= GeneratedSymbolData{ GeneratedSymbolData::DefaultDestructor{
								field_type.getType() } },
						});

						std::vector<Box<code::Expr>> args;
						args.emplace_back(makeBox<code::RefOfExpr>(
							ctx,
							code::generatedOrigin(),
							makeBox<code::AccessExpr>(
								ctx,
								code::generatedOrigin(),
								dereffed_self->clone(),
								field.getSymbol()
							)
						));

						body.emplace_back(makeBox<code::ExprStmt>(
							code::generatedOrigin(),
							makeBox<code::CallExpr>(
								ctx,
								code::generatedOrigin(),
								makeBox<code::IdentifierExpr>(
									ctx, code::generatedOrigin(), field_dtor_sym
								),
								std::move(args)
							)
						));
					}
				}
				break;
			}
			default:
				// Currently no body logic is generated for the default destructor.
				// This stub just provides an empty destructor.
				break;
			}

			return HOUTFunction(
				code::generatedOrigin(),
				&dtor_decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDefaultDestructor);
}

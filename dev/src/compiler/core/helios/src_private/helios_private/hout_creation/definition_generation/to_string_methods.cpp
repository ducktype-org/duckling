#include "to_string_methods.hpp"

#include "helios/symbols/symbol_id_utils.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	struct IMPLEMENT_QUERY(QueryToStringMethod, query::QResult<HOUTFunction>) {
		static void stringifyBool(
			Context&                       ctx,
			const QKey                     owner_type,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;

			const auto string_type = tsh::SymbolType<>::withDefaults(tsh::getStringType());
			const auto bool_type   = tsh::SymbolType<>::withDefaults(owner_type);

			const auto builtin_sym = ctx.query<QueryGeneratedSymbol>({
				.name                  = base::StrID("builtin_stringify_bool"),
				.generated_symbol_data = GeneratedSymbolData{ GeneratedSymbolData::BuiltinOperator{
					.operator_type = ctx.query<tsh::QueryFunctionType>({
						{ bool_type },
						string_type,
					}),
				} },
			});

			std::vector<Box<code::Expr>> args;
			args.emplace_back(makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param)
			);

			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::CallExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), builtin_sym),
					std::move(args)
				)
			));
		}

		static void stringifyIntegral(
			Context&                       ctx,
			const QKey                     owner_type,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;

			const auto string_type = tsh::SymbolType<>::withDefaults(tsh::getStringType());
			const auto int_type    = owner_type.as<tsh::IntegralAbstractType>();

			const bool is_signed
				= int_type.getSignedness() == tsh::IntegralAbstractType::Signedness::Signed;

			const auto target_builtin_name = is_signed ? base::StrID("builtin_stringify_i64")
			                                           : base::StrID("builtin_stringify_u64");

			const auto target_int_type = tsh::SymbolType<>::withDefaults(tsh::getIntegralType(
				ctx,
				64,
				is_signed ? tsh::IntegralAbstractType::Signedness::Signed
						  : tsh::IntegralAbstractType::Signedness::Unsigned
			));

			const auto builtin_sym = ctx.query<QueryGeneratedSymbol>({
				.name                  = target_builtin_name,
				.generated_symbol_data = GeneratedSymbolData{ GeneratedSymbolData::BuiltinOperator{
					.operator_type = ctx.query<tsh::QueryFunctionType>({
						{ target_int_type },
						string_type,
					}),
				} },
			});

			Box<code::Expr> arg_expr
				= makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param);

			const auto owner_sym_type = tsh::SymbolType<>::withDefaults(owner_type);
			const auto coercion_res
				= canCoerce(ctx, owner_sym_type, target_int_type).valueOrThrow();
			if (!coercion_res.getCoercion().isEmptyCoercion())
				arg_expr = coercion_res.coerce(ctx, std::move(arg_expr));

			std::vector<Box<code::Expr>> args;
			args.emplace_back(std::move(arg_expr));

			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::CallExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), builtin_sym),
					std::move(args)
				)
			));
		}

		static void stringifyFloat(
			Context&                       ctx,
			const QKey                     owner_type,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;

			const auto string_type = tsh::SymbolType<>::withDefaults(tsh::getStringType());
			const auto float_type  = tsh::SymbolType<>::withDefaults(owner_type);

			const auto target_builtin_name = base::StrID("builtin_stringify_f64");
			const auto target_float_type
				= tsh::SymbolType<>::withDefaults(tsh::getFloatType(ctx, 64));

			const auto builtin_sym = ctx.query<QueryGeneratedSymbol>({
				.name                  = target_builtin_name,
				.generated_symbol_data = GeneratedSymbolData{ GeneratedSymbolData::BuiltinOperator{
					.operator_type = ctx.query<tsh::QueryFunctionType>({
						{ target_float_type },
						string_type,
					}),
				} },
			});

			Box<code::Expr> arg_expr
				= makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param);

			const auto owner_sym_type = tsh::SymbolType<>::withDefaults(owner_type);
			const auto coercion_res
				= canCoerce(ctx, owner_sym_type, target_float_type).valueOrThrow();
			if (!coercion_res.getCoercion().isEmptyCoercion())
				arg_expr = coercion_res.coerce(ctx, std::move(arg_expr));

			std::vector<Box<code::Expr>> args;
			args.emplace_back(std::move(arg_expr));

			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::CallExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), builtin_sym),
					std::move(args)
				)
			));
		}

		static void stringifyChar(
			Context&                       ctx,
			const QKey                     owner_type,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;

			const auto string_type = tsh::SymbolType<>::withDefaults(tsh::getStringType());
			const auto char_type   = tsh::SymbolType<>::withDefaults(owner_type);

			const auto builtin_sym = ctx.query<QueryGeneratedSymbol>({
				.name                  = base::StrID("builtin_stringify_char"),
				.generated_symbol_data = GeneratedSymbolData{ GeneratedSymbolData::BuiltinOperator{
					.operator_type = ctx.query<tsh::QueryFunctionType>({
						{ char_type },
						string_type,
					}),
				} },
			});

			std::vector<Box<code::Expr>> args;
			args.emplace_back(makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param)
			);

			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::CallExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), builtin_sym),
					std::move(args)
				)
			));
		}

		static void stringifyClass(
			Context&                       ctx,
			const QKey                     owner_type,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto class_type      = owner_type.as<tsh::ClassAbstractType>();
			const auto class_name      = name(class_type.getSymbol());
			const auto class_interface = class_type.getInterface(ctx);

			// Grab the symbol of the `concatenated` built-in function
			const auto string_type = tsh::SymbolType<>::withDefaults(tsh::getStringType());
			const auto concat_sym  = ctx.query<QueryGeneratedSymbol>({
				 .name                  = base::StrID("builtin_string_concatenated"),
				 .generated_symbol_data = GeneratedSymbolData{ GeneratedSymbolData::BuiltinOperator{
					 .operator_type = ctx.query<tsh::QueryFunctionType>({
                        { string_type, string_type },
                        string_type,
                    }),
                } },
            });

			// Prelude: the class name and opening parenthesis
			const auto result_sym = ctx.query<QueryGeneratedSymbol>(
				{ .name                  = base::StrID("__result"),
			      .generated_symbol_data = GeneratedSymbolData{ GeneratedSymbolData::Variable{
					  .function_symbol = to_string_decl.original_symbol,
					  .variable_index  = 0,
					  .type            = string_type } } }
			);
			body.emplace_back(makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				makeBox<code::LiteralStringExpr>(
					ctx, code::generatedOrigin(), base::StrID(class_name.str() + "(")
				),
				string_type,
				result_sym
			));

			// Main body: append the fields
			const std::vector<tsh::InterfaceElement> fields
				= class_interface->getFieldsView() | std::ranges::to<std::vector>();
			const auto num_fields = fields.size();

			for (usize idx = 0; idx < num_fields; idx++) {
				// Get the stringified field
				const auto& field      = fields.at(idx);
				const auto  field_type = field.getType(ctx);

				const SymID field_to_string_sym = ctx.query<QueryGeneratedSymbol>({
					.name = base::StrID("toString"),
					.generated_symbol_data
					= GeneratedSymbolData{ GeneratedSymbolData::ToStringMethod{
						field_type.getType() } },
				});

				auto accessed_field = makeBox<code::AccessExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(
						ctx, code::generatedOrigin(), to_string_decl.parameters.at(0).helios_symbol
					),
					field.getSymbol()
				);

				std::vector<Box<code::Expr>> v1;
				v1.emplace_back(std::move(accessed_field));

				auto stringified_field = makeBox<code::CallExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), field_to_string_sym),
					std::move(v1)
				);

				// Concatenate the stringified field
				std::vector<Box<code::Expr>> v2;
				v2.emplace_back(
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym)
				);
				v2.emplace_back(std::move(std::move(stringified_field)));
				body.emplace_back(makeBox<code::AssignmentStmt>(
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym),
					makeBox<code::CallExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), concat_sym),
						std::move(v2)
					)
				));

				// Concatenate the separator or closing parenthesis
				std::vector<Box<code::Expr>> v3;
				v3.emplace_back(
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym)
				);
				v3.emplace_back(makeBox<code::LiteralStringExpr>(
					ctx, code::generatedOrigin(), base::StrID(idx < num_fields - 1 ? "," : ")")
				));
				body.emplace_back(makeBox<code::AssignmentStmt>(
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym),
					makeBox<code::CallExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), concat_sym),
						std::move(v3)
					)
				));
			}

			// Finally, return
			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym)
			));
		}

		static PResult provide(Context& ctx, const QKey owner_type) {
			const auto  to_string_sym  = ctx.query<QueryGeneratedSymbol>({
                base::StrID("toString"),
                GeneratedSymbolData{ GeneratedSymbolData::ToStringMethod{ owner_type } },
            });
			const auto& to_string_decl = ctx.query<QueryDeclOfFun>(to_string_sym)->valueOrThrow();

			std::vector<Box<code::Stmt>> body{};

			switch (owner_type.getKind()) {
			case tsh::Kind::Integral: {
				stringifyIntegral(ctx, owner_type, to_string_decl, body);
				break;
			}
			case tsh::Kind::Float: {
				stringifyFloat(ctx, owner_type, to_string_decl, body);
				break;
			}
			case tsh::Kind::Char: {
				stringifyChar(ctx, owner_type, to_string_decl, body);
				break;
			}
			case tsh::Kind::Bool: {
				stringifyBool(ctx, owner_type, to_string_decl, body);
				break;
			}
			case tsh::Kind::Class: {
				stringifyClass(ctx, owner_type, to_string_decl, body);
				break;
			}
			default: {
				std::string msg
					= "Stringification not yet implemented for " + owner_type.toString();
				body.emplace_back(makeBox<code::ReturnStmt>(
					code::generatedOrigin(),
					makeBox<code::LiteralStringExpr>(ctx, code::generatedOrigin(), base::StrID(msg))
				));
				break;
			}
			}

			return HOUTFunction(
				code::generatedOrigin(),
				&to_string_decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryToStringMethod);
}

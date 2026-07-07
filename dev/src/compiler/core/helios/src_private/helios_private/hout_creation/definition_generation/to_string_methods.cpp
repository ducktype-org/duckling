#include "to_string_methods.hpp"

#include "helios/symbols/lang_primitives.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	SymID toStringSymForType(query::Context& ctx, const tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>({
			.name = base::StrID("toString"),
			.generated_symbol_data
			= GeneratedSymbolData{ GeneratedSymbolData::ToStringMethod{ type } },
		});
	}

	namespace {
		inline const auto STRING_TYPE = tsh::SymbolType<>::withDefaults(tsh::getStringType());

		SymID stringifySym(
			query::Context& ctx, const tsh::SymbolType<>& type, const std::string& name
		) {
			return ctx.query<QueryGeneratedSymbol>({
				.name                  = base::StrID(name),
				.generated_symbol_data = GeneratedSymbolData{ GeneratedSymbolData::BuiltinOperator{
					.operator_type = ctx.query<tsh::QueryFunctionType>({
						{ type },
						STRING_TYPE,
					}),
				} },
			});
		}
	}

	SymID concatSym(query::Context& ctx) {
		return ctx.query<QueryGeneratedSymbol>({
			.name                  = base::StrID("builtin_string_concatenated"),
			.generated_symbol_data = GeneratedSymbolData{ GeneratedSymbolData::BuiltinOperator{
				.operator_type = ctx.query<tsh::QueryFunctionType>({
					{ STRING_TYPE, STRING_TYPE },
					STRING_TYPE,
				}),
			} },
		});
	}

	Box<code::Expr> getStringFromLiteralExpr(query::Context& ctx, base::StrID value) {
		std::vector<Box<code::Expr>> call_args;
		call_args.emplace_back(
			makeBox<code::LiteralStringExpr>(ctx, code::generatedOrigin(), value)
		);
		SymID callee_sym = stringifySym(
			ctx, tsh::SymbolType<>::withDefaults(tsh::getCharSliceType(ctx)), "builtin_stringify_str"
		);
		auto callee = makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), callee_sym);
		return makeBox<code::CallExpr>(
			ctx, code::generatedOrigin(), std::move(callee), std::move(call_args)
		);
	}

	struct IMPLEMENT_QUERY(QueryToStringMethod, query::QResult<HOUTFunction>) {
		static void stringifyBool(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param  = to_string_decl.parameters.at(0).helios_symbol;
			const auto bool_type   = to_string_decl.parameters.at(0).type;
			const auto builtin_sym = stringifySym(ctx, bool_type, "builtin_stringify_bool");

			std::vector<Box<code::Expr>> args;
			args.emplace_back(
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param)
			);

			body.emplace_back(
				makeBox<code::ReturnStmt>(
					code::generatedOrigin(),
					makeBox<code::CallExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), builtin_sym),
						std::move(args)
					)
				)
			);
		}

		static void stringifyIntegral(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param      = to_string_decl.parameters.at(0).helios_symbol;
			const auto source_int_type = to_string_decl.parameters.at(0).type;
			const bool is_signed
				= source_int_type.getType().as<tsh::IntegralAbstractType>().getSignedness()
			   == tsh::IntegralAbstractType::Signedness::Signed;
			const auto target_builtin_name
				= is_signed ? "builtin_stringify_i64" : "builtin_stringify_u64";
			const auto target_int_type = tsh::SymbolType<>::withDefaults(
				tsh::getIntegralType(
					ctx,
					64,
					is_signed ? tsh::IntegralAbstractType::Signedness::Signed
							  : tsh::IntegralAbstractType::Signedness::Unsigned
				)
			);
			const auto builtin_sym = stringifySym(ctx, target_int_type, target_builtin_name);

			Box<code::Expr> arg_expr
				= makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param);

			const auto coercion_res
				= canCoerce(ctx, source_int_type, target_int_type).valueOrThrow();
			if (!coercion_res.getCoercion().isEmptyCoercion())
				arg_expr = coercion_res.coerce(ctx, std::move(arg_expr));

			std::vector<Box<code::Expr>> args;
			args.emplace_back(std::move(arg_expr));

			body.emplace_back(
				makeBox<code::ReturnStmt>(
					code::generatedOrigin(),
					makeBox<code::CallExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), builtin_sym),
						std::move(args)
					)
				)
			);
		}

		static void stringifyFloat(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param        = to_string_decl.parameters.at(0).helios_symbol;
			const auto source_float_type = to_string_decl.parameters.at(0).type;
			const auto target_float_type
				= tsh::SymbolType<>::withDefaults(tsh::getFloatType(ctx, 64));
			const auto builtin_sym = stringifySym(ctx, target_float_type, "builtin_stringify_f64");

			Box<code::Expr> arg_expr
				= makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param);

			const auto coercion_res
				= canCoerce(ctx, source_float_type, target_float_type).valueOrThrow();
			if (!coercion_res.getCoercion().isEmptyCoercion())
				arg_expr = coercion_res.coerce(ctx, std::move(arg_expr));

			std::vector<Box<code::Expr>> args;
			args.emplace_back(std::move(arg_expr));

			body.emplace_back(
				makeBox<code::ReturnStmt>(
					code::generatedOrigin(),
					makeBox<code::CallExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), builtin_sym),
						std::move(args)
					)
				)
			);
		}

		static void stringifyChar(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param       = to_string_decl.parameters.at(0).helios_symbol;
			const auto source_char_type = to_string_decl.parameters.at(0).type;
			const auto builtin_sym = stringifySym(ctx, source_char_type, "builtin_stringify_char");

			std::vector<Box<code::Expr>> args;
			args.emplace_back(
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param)
			);

			body.emplace_back(
				makeBox<code::ReturnStmt>(
					code::generatedOrigin(),
					makeBox<code::CallExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), builtin_sym),
						std::move(args)
					)
				)
			);
		}

		static void stringifyString(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;

			body.emplace_back(
				makeBox<code::ReturnStmt>(
					code::generatedOrigin(),
					makeBox<code::DerefExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param)
					)
				)
			);
		}

		static void stringifySlice(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			auto& self_param         = to_string_decl.parameters.at(0);
			auto  slice_type         = self_param.type.getType().as<tsh::SliceAbstractType>();
			auto  slice_element_type = slice_type.getElementType();
			if (slice_element_type.getType() == tsh::getCharType()
			    and slice_element_type.getRefKind() == tsh::ReferenceKind::Direct) {
				SymID callee_sym = stringifySym(
					ctx,
					tsh::SymbolType<>::withDefaults(tsh::getCharSliceType(ctx)),
					"builtin_stringify_str"
				);
				auto callee
					= makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), callee_sym);

				std::vector<Box<code::Expr>> call_args;
				call_args.emplace_back(
					makeBox<code::IdentifierExpr>(
						ctx, code::generatedOrigin(), self_param.helios_symbol
					)
				);

				body.emplace_back(
					makeBox<code::ReturnStmt>(
						code::generatedOrigin(),
						makeBox<code::CallExpr>(
							ctx, code::generatedOrigin(), std::move(callee), std::move(call_args)
						)
					)
				);
			} else {
				ctx.logInt(
					makeBox<dia_int::NotYetImplementedCodeError>(
						"toString for non-string slices not yet implemented."
					)
				);
			}
		}

		static void stringifyUnit(
			Context& ctx,
			const HOUTFunctionDeclaration& /* to_string_decl */,
			std::vector<Box<code::Stmt>>& body
		) {
			body.emplace_back(
				makeBox<code::ReturnStmt>(
					code::generatedOrigin(), getStringFromLiteralExpr(ctx, base::StrID("()"))
				)
			);
		}

		static void stringifyAggregate(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body,
			const base::StrID              prefix,
			const CRef<tsh::TypeInterface> type_interface
		) {
			const auto concat_sym = concatSym(ctx);
			// Create a reusable expression of the de-reffed self (self is passed by reference)
			auto self_expr = makeBox<code::DerefExpr>(
				ctx,
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(
					ctx, code::generatedOrigin(), to_string_decl.parameters.at(0).helios_symbol
				)
			);
			auto reusable_self_expr = makeBox<code::ReusableExpr>(ctx, std::move(self_expr), true);

			// Prelude: the class name and opening parenthesis
			const auto result_sym = ctx.query<QueryGeneratedSymbol>(
				{ .name = base::StrID("__result"),
			      .generated_symbol_data
			      = GeneratedSymbolData{ GeneratedSymbolData::GeneratedFunctionVariable{
					  .function_symbol = to_string_decl.original_symbol,
					  .variable_index  = 0,
					  .type            = STRING_TYPE } } }
			);
			body.emplace_back(
				makeBox<code::VariableStmt>(
					code::generatedOrigin(),
					getStringFromLiteralExpr(ctx, base::StrID(prefix)),
					STRING_TYPE,
					result_sym
				)
			);

			// Main body: append the fields
			const std::vector<tsh::InterfaceElement> fields
				= type_interface->getFieldsView() | std::ranges::to<std::vector>();
			const auto num_fields = fields.size();

			for (usize idx = 0; idx < num_fields; idx++) {
				// Get the stringified field
				const auto& field               = fields.at(idx);
				const auto  field_type          = field.getType(ctx);
				const SymID field_to_string_sym = toStringSymForType(ctx, field_type.getType());

				auto            next_reusable_self_expr = reusable_self_expr->nextUse();
				Box<code::Expr> accessed_field          = makeBox<code::AccessExpr>(
					ctx, code::generatedOrigin(), std::move(reusable_self_expr), field.getSymbol()
				);
				// If the field is not a simple type, we must call its `toString` method on a reference.
				if (not accessed_field->expression_type.getType().isSimple()
				    and accessed_field->expression_type.getSymbolType().getRefKind()
				            != tsh::ReferenceKind::Ref) {
					accessed_field = makeBox<code::RefOfExpr>(
						ctx, code::generatedOrigin(), std::move(accessed_field)
					);
				}
				// But also if the accessed field is a reference to a simple type, we must deref it.
				if (accessed_field->expression_type.getType().isSimple()
				    and accessed_field->expression_type.getSymbolType().getRefKind()
				            != tsh::ReferenceKind::Direct) {
					accessed_field = makeBox<code::DerefExpr>(
						ctx, code::generatedOrigin(), std::move(accessed_field)
					);
				}
				reusable_self_expr = std::move(next_reusable_self_expr);

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
				body.emplace_back(
					makeBox<code::AssignmentStmt>(
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym),
						makeBox<code::CallExpr>(
							ctx,
							code::generatedOrigin(),
							makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), concat_sym),
							std::move(v2)
						)
					)
				);

				// Concatenate the separator or closing parenthesis
				std::vector<Box<code::Expr>> v3;
				v3.emplace_back(
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym)
				);
				v3.emplace_back(
					getStringFromLiteralExpr(ctx, base::StrID(idx < num_fields - 1 ? "," : ")"))
				);
				body.emplace_back(
					makeBox<code::AssignmentStmt>(
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym),
						makeBox<code::CallExpr>(
							ctx,
							code::generatedOrigin(),
							makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), concat_sym),
							std::move(v3)
						)
					)
				);
			}

			// Finally, return
			body.emplace_back(
				makeBox<code::ReturnStmt>(
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym)
				)
			);
		}

		static void stringifyTuple(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto tuple_type
				= to_string_decl.parameters.at(0).type.getType().as<tsh::TupleAbstractType>();
			const auto tuple_interface = tuple_type.getInterface(ctx);

			stringifyAggregate(ctx, to_string_decl, body, base::StrID("("), tuple_interface);
		}

		static void stringifyClass(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto class_type
				= to_string_decl.parameters.at(0).type.getType().as<tsh::ClassAbstractType>();
			const auto class_name      = name(class_type.getSymbol());
			const auto class_interface = class_type.getInterface(ctx);

			stringifyAggregate(
				ctx, to_string_decl, body, base::StrID(class_name.str() + "("), class_interface
			);
		}

		static PResult provide(Context& ctx, const QKey owner_type) {
			const auto  to_string_sym  = toStringSymForType(ctx, owner_type);
			const auto& to_string_decl = ctx.query<QueryDeclOfFun>(to_string_sym)->valueOrThrow();

			std::vector<Box<code::Stmt>> body{};

			switch (owner_type.getKind()) {
			case tsh::Kind::Integral: {
				stringifyIntegral(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Float: {
				stringifyFloat(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Char: {
				stringifyChar(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Bool: {
				stringifyBool(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::String: {
				stringifyString(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Slice: {
				stringifySlice(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Unit: {
				stringifyUnit(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Tuple: {
				stringifyTuple(ctx, to_string_decl, body);
				break;
			}
			case tsh::Kind::Class: {
				stringifyClass(ctx, to_string_decl, body);
				break;
			}
			default: {
				std::string msg
					= "Stringification not yet implemented for " + owner_type.toString();
				body.emplace_back(
					makeBox<code::ReturnStmt>(
						code::generatedOrigin(), getStringFromLiteralExpr(ctx, base::StrID(msg))
					)
				);
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

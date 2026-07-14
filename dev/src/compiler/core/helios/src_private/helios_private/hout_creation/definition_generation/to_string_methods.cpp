#include "to_string_methods.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/lang_primitives.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/lookup/lookup_result.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	SymID toStringSymForType(query::Context& ctx, const tsh::AbstractType type) {
		for (auto& elem: type.getInterface(ctx)->getElementsWithName(base::StrID("toString")))
			if (elem.specialKind() == tsh::InterfaceElement::SpecialKind::ToString)
				return elem.getSymbol();
		CORE_PANIC("Every symbol should have toString.");
	}

	SymID generatedToStringSymForType(query::Context& ctx, const tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>({
			.name                  = base::StrID("toString"),
			.generated_symbol_data = Method{ .owner_type = type, .kind = Method::Kind::ToString },
		});
	}

#define STRING_TYPE tsh::SymbolType<>::withDefaults(tsh::getStringType(ctx))
#define LANG_PRIMITIVE(primitive) \
	ctx.query<QueryLanguagePrimitiveSymID>({ primitive })->valueOrThrow()

	Box<code::Expr> getStringFromLiteralExpr(query::Context& ctx, base::StrID value) {
		std::vector<Box<code::Expr>> call_args;
		call_args.emplace_back(makeBox<code::LiteralStringExpr>(ctx, code::generatedOrigin(), value)
		);
		SymID callee_sym = LANG_PRIMITIVE(LanguagePrimitive::StringifyStr);
		auto  callee     = makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), callee_sym);
		return makeBox<code::CallExpr>(
			ctx, code::generatedOrigin(), std::move(callee), std::move(call_args)
		);
	}

	SymID stringAppendMethodSym(query::Context& ctx, const bool arg_by_reference) {
		// Look up the `append` method on the `String` class. `append` is overloaded on a `String`
		// argument, so we select either the by-reference (`append(other: ref String)`) or the
		// by-value (`append(other: String)`) overload depending on `arg_by_reference`.
		auto       string_abstract_type = tsh::getStringType(ctx);
		const auto expected_arg_type
			= tsh::SymbolType<>::withDefaults(string_abstract_type)
		          .withReferenceKind(
					  arg_by_reference ? tsh::ReferenceKind::Ref : tsh::ReferenceKind::Direct
				  );
		const auto& lookup_result = HInterface::ofTypeInstance(string_abstract_type)
		                                .lookup(ctx, base::StrID("append"))
		                                ->valueOrThrow();


		for (const SymID candidate: lookup_result.leaves) {
			const auto fn_type = ctx.query<QueryTypeOfSymbol>(candidate)
			                         ->valueOrThrow()
			                         .getType()
			                         .as<tsh::FunctionAbstractType>();
			const auto& params = fn_type.getParameterTypes();
			if (params.size() == 2 && params.at(1) == expected_arg_type) return candidate;
		}
		ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
			base::strConcat(
				"The String class language primitive doesn't have the `append(",
				arg_by_reference ? "ref " : "",
				"String)` method."
			),
			"This method is required for the compiler to work."
		));
		query::throwFailed();
		CORE_UNREACHABLE();
	}

	struct IMPLEMENT_QUERY(QueryToStringMethod, query::QResult<HOUTFunction>) {
		static void stringifyBool(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param  = to_string_decl.parameters.at(0).helios_symbol;
			const auto builtin_sym = LANG_PRIMITIVE(LanguagePrimitive::StringifyBool);

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
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param      = to_string_decl.parameters.at(0).helios_symbol;
			const auto source_int_type = to_string_decl.parameters.at(0).type;
			const bool is_signed
				= source_int_type.getType().as<tsh::IntegralAbstractType>().getSignedness()
			   == tsh::IntegralAbstractType::Signedness::Signed;
			const auto target_int_type = tsh::SymbolType<>::withDefaults(tsh::getIntegralType(
				ctx,
				64,
				is_signed ? tsh::IntegralAbstractType::Signedness::Signed
						  : tsh::IntegralAbstractType::Signedness::Unsigned
			));
			const auto builtin_sym     = LANG_PRIMITIVE(
                is_signed ? LanguagePrimitive::StringifyI64 : LanguagePrimitive::StringifyU64
			);

			Box<code::Expr> arg_expr
				= makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param);

			const auto coercion_res
				= canCoerce(ctx, arg_expr->expression_type, target_int_type).valueOrThrow();
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
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;
			const auto target_float_type
				= tsh::SymbolType<>::withDefaults(tsh::getFloatType(ctx, 64));
			const auto builtin_sym = LANG_PRIMITIVE(LanguagePrimitive::StringifyF64);

			Box<code::Expr> arg_expr
				= makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param);

			const auto coercion_res
				= canCoerce(ctx, arg_expr->expression_type, target_float_type).valueOrThrow();
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
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param  = to_string_decl.parameters.at(0).helios_symbol;
			const auto builtin_sym = LANG_PRIMITIVE(LanguagePrimitive::StringifyChar);

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

		static void stringifyString(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;

			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::DerefExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), self_param)
				)
			));
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
				SymID callee_sym = LANG_PRIMITIVE(LanguagePrimitive::StringifyStr);
				auto  callee
					= makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), callee_sym);

				std::vector<Box<code::Expr>> call_args;
				call_args.emplace_back(makeBox<code::IdentifierExpr>(
					ctx, code::generatedOrigin(), self_param.helios_symbol
				));

				body.emplace_back(makeBox<code::ReturnStmt>(
					code::generatedOrigin(),
					makeBox<code::CallExpr>(
						ctx, code::generatedOrigin(), std::move(callee), std::move(call_args)
					)
				));
			} else {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"toString for non-string slices not yet implemented."
				));
			}
		}

		static void stringifyUnit(
			Context& ctx,
			const HOUTFunctionDeclaration& /* to_string_decl */,
			std::vector<Box<code::Stmt>>& body
		) {
			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(), getStringFromLiteralExpr(ctx, base::StrID("()"))
			));
		}

		static void stringifyAggregate(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body,
			const base::StrID              prefix,
			const CRef<tsh::TypeInterface> type_interface
		) {
			const auto append_sym = stringAppendMethodSym(ctx, true);
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
			      = GeneratedFunctionVariable{ .function_symbol = to_string_decl.original_symbol,
			                                   .variable_index  = 0,
			                                   .type            = STRING_TYPE } }
			);
			body.emplace_back(makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				getStringFromLiteralExpr(ctx, base::StrID(prefix)),
				STRING_TYPE,
				result_sym
			));

			// `result.append(<other>)` — mutates `result` in place instead of building a new
			// concatenated String. `append(other: ref String)` takes both `self` and `other` by
			// reference, so the arguments are passed as references.
			const auto append_result_stmt = [&](Box<code::Expr> other) -> Box<code::Stmt> {
				std::vector<Box<code::Expr>> args;
				args.emplace_back(makeBox<code::RefOfExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym)
				));
				args.emplace_back(
					makeBox<code::RefOfExpr>(ctx, code::generatedOrigin(), std::move(other))
				);
				return makeBox<code::ExprStmt>(
					code::generatedOrigin(),
					makeBox<code::CallExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), append_sym),
						std::move(args)
					)
				);
			};

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

				// Append the stringified field.
				body.emplace_back(append_result_stmt(std::move(stringified_field)));

				// Append the separator or closing parenthesis.
				body.emplace_back(append_result_stmt(
					getStringFromLiteralExpr(ctx, base::StrID(idx < num_fields - 1 ? "," : ")"))
				));
			}

			// Finally, return
			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_sym)
			));
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
				body.emplace_back(makeBox<code::ReturnStmt>(
					code::generatedOrigin(), getStringFromLiteralExpr(ctx, base::StrID(msg))
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

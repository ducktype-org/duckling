#include "to_string_methods.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	using namespace code::shorthands;

	/**
	 * @brief Get the symbol of the `toString` method for a given type.
	 */
	SymID toStringSymForType(query::Context& ctx, const tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>({
			.name                  = base::StrID("toString"),
			.generated_symbol_data = Method{ .owner_type = type, .kind = Method::Kind::ToString },
		});
	}

	namespace {
		inline const auto STRING_TYPE = tsh::SymbolType<>::withDefaults(tsh::getStringType());

		/**
		 * @brief Get the symbol of the `builtin_stringify_X` function for given type X.
		 */
		SymID stringifySym(
			query::Context& ctx, const tsh::SymbolType<>& type, const std::string& name
		) {
			return ctx.query<QueryGeneratedSymbol>({
				.name                  = base::StrID(name),
				.generated_symbol_data = BuiltinOperator{
					.operator_type = ctx.query<tsh::QueryFunctionType>({
						{ type },
						STRING_TYPE,
					}),
					.operatoriness = HOUTFunctionDeclaration::Operatoriness::None,
				},
			});
		}
	}

	/**
	 * @brief Get the symbol of string concatenation.
	 */
	SymID concatSym(query::Context& ctx) {
		return ctx.query<QueryGeneratedSymbol>({
			.name                  = base::StrID("builtin_string_concatenated"),
			.generated_symbol_data = BuiltinOperator{
				.operator_type = ctx.query<tsh::QueryFunctionType>({
					{ STRING_TYPE, STRING_TYPE },
					STRING_TYPE,
				}),
				.operatoriness = HOUTFunctionDeclaration::Operatoriness::None,
			},
		});
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

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.call(s.ident(builtin_sym), s.ident(self_param))));
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
			const auto target_int_type = tsh::SymbolType<>::withDefaults(tsh::getIntegralType(
				ctx,
				64,
				is_signed ? tsh::IntegralAbstractType::Signedness::Signed
						  : tsh::IntegralAbstractType::Signedness::Unsigned
			));
			const auto builtin_sym     = stringifySym(ctx, target_int_type, target_builtin_name);

			const Shorthand s{ ctx };
			body.emplace_back(
				s.ret(s.call(s.ident(builtin_sym), s.coerce(s.ident(self_param), target_int_type)))
			);
		}

		static void stringifyFloat(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;
			const auto target_float_type
				= tsh::SymbolType<>::withDefaults(tsh::getFloatType(ctx, 64));
			const auto builtin_sym = stringifySym(ctx, target_float_type, "builtin_stringify_f64");

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(
				s.call(s.ident(builtin_sym), s.coerce(s.ident(self_param), target_float_type))
			));
		}

		static void stringifyChar(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param       = to_string_decl.parameters.at(0).helios_symbol;
			const auto source_char_type = to_string_decl.parameters.at(0).type;
			const auto builtin_sym = stringifySym(ctx, source_char_type, "builtin_stringify_char");

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.call(s.ident(builtin_sym), s.ident(self_param))));
		}

		static void stringifyString(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body
		) {
			const auto self_param = to_string_decl.parameters.at(0).helios_symbol;

			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.deref(s.ident(self_param))));
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
				const SymID callee_sym = stringifySym(
					ctx,
					tsh::SymbolType<>::withDefaults(tsh::getCharSliceType(ctx)),
					"builtin_stringify_str"
				);

				const Shorthand s{ ctx };
				body.emplace_back(
					s.ret(s.call(s.ident(callee_sym), s.ident(self_param.helios_symbol)))
				);
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
			const Shorthand s{ ctx };
			body.emplace_back(s.ret(s.litStrObj(base::StrID("()"))));
		}

		static void stringifyAggregate(
			Context&                       ctx,
			const HOUTFunctionDeclaration& to_string_decl,
			std::vector<Box<code::Stmt>>&  body,
			const base::StrID              prefix,
			const CRef<tsh::TypeInterface> type_interface
		) {
			const Shorthand s{ ctx };
			const auto      concat_sym = concatSym(ctx);
			// Create a reusable expression of the de-reffed self (self is passed by reference)
			auto reusable_self_expr
				= s.reusable(s.deref(s.ident(to_string_decl.parameters.at(0).helios_symbol)));

			// Prelude: the class name and opening parenthesis
			const auto result_sym = ctx.query<QueryGeneratedSymbol>(
				{ .name = base::StrID("__result"),
			      .generated_symbol_data
			      = GeneratedFunctionVariable{ .function_symbol = to_string_decl.original_symbol,
			                                   .variable_index  = 0,
			                                   .type            = STRING_TYPE } }
			);
			body.emplace_back(s.var(result_sym, STRING_TYPE, s.litStrObj(base::StrID(prefix))));

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
				Box<code::Expr> accessed_field
					= s.prepToPassSelf(s.access(std::move(reusable_self_expr), field.getSymbol()));
				reusable_self_expr = std::move(next_reusable_self_expr);

				auto stringified_field
					= s.call(s.ident(field_to_string_sym), std::move(accessed_field));

				// Concatenate the stringified field, then the separator / closing parenthesis.
				body.emplace_back(s.assign(
					s.ident(result_sym),
					s.call(s.ident(concat_sym), s.ident(result_sym), std::move(stringified_field))
				));
				body.emplace_back(s.assign(
					s.ident(result_sym),
					s.call(
						s.ident(concat_sym),
						s.ident(result_sym),
						s.litStrObj(base::StrID(idx < num_fields - 1 ? "," : ")"))
					)
				));
			}

			// Finally, return
			body.emplace_back(s.ret(s.ident(result_sym)));
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
				const Shorthand s{ ctx };
				std::string     msg
					= "Stringification not yet implemented for " + owner_type.toString();
				body.emplace_back(s.ret(s.litStrObj(base::StrID(msg))));
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

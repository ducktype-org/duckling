#include "default_copy_constructors.hpp"

#include "helios/tsh/kind.hpp"
#include "helios/tsh/types.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/hout_creation/definition_generation/length_methods.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include "base/pointers/box.hpp"

#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	SymID copySymForType(query::Context& ctx, const tsh::AbstractType type) {
		// First, try to get the user-defined constructor.
		if (type.getKind() == tsh::Kind::Class) {
			if (const auto user
			    = userCopyConstructorOf(ctx, type.as<tsh::ClassAbstractType>().getSymbol()))
				return user.value();
		}

		// Otherwise, we use the default one.
		return ctx.query<QueryGeneratedSymbol>({
			.name = base::StrID("__copy"),
			.generated_symbol_data
			= GeneratedSymbolData{ GeneratedSymbolData::DefaultCopyConstructor{ type } },
		});
	}

	namespace {
		/**
		 * @brief Build a HOUT expression that produces a copy of `source`.
		 *
		 * - Trivially-copyable sources are returned as they where (which means they are byte
		 * copied).
		 * - Non-trivially-copyable class, static-array and tuple members are copied by calling
		 * their own default copy constructor with a reference to `source`.
		 */
		Box<code::Expr> makeCopyExpr(
			query::Context& ctx, Box<code::Expr> source, const tsh::SymbolType<>& type
		) {
			std::cout << "Make copy expr\n";
			if (type.isTriviallyCopyable(ctx)) return source;

			const auto abstract_type = type.getType();
			if (type.getRefKind() == tsh::ReferenceKind::Direct
			    && (abstract_type.getKind() == tsh::Kind::Class
			        || abstract_type.getKind() == tsh::Kind::StaticArray
			        || abstract_type.getKind() == tsh::Kind::Tuple)) {
				const SymID copy_sym = copySymForType(ctx, abstract_type);

				std::vector<Box<code::Expr>> args;
				args.emplace_back(
					makeBox<code::RefOfExpr>(ctx, code::generatedOrigin(), std::move(source))
				);

				return makeBox<code::CallExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), copy_sym),
					std::move(args)
				);
			}

			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
				base::strConcat(
					"Default copy constructor for non-trivially-copyable type `",
					type.toString(),
					"`."
				),
				""
			));
			query::throwFailed();
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Build `(*source).<member>` - a dereference of the `source` parameter followed by a
		 * field access.
		 */
		Box<code::Expr> derefSourceField(query::Context& ctx, SymID source_symbol, SymID field) {
			return makeBox<code::AccessExpr>(
				ctx,
				code::generatedOrigin(),
				makeBox<code::DerefExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), source_symbol)
				),
				field
			);
		}

		// Builds the copy-constructor body for a class or tuple.
		std::vector<Box<code::Stmt>> buildAggregateCopyBody(
			query::Context&          ctx,
			const tsh::AbstractType& owner_type,
			const SymID              copy_sym,
			const SymID              source_symbol,
			const tsh::SymbolType<>& result_symbol_type
		) {
			using Variable = GeneratedSymbolData::GeneratedFunctionVariable;

			const std::vector<tsh::InterfaceElement> fields
				= owner_type.getInterface(ctx)->getFieldsView() | std::ranges::to<std::vector>();

			std::vector<Box<code::Stmt>> body;
			body.reserve(1 + fields.size() + 1);

			// @TODO: #2307 Classes with a field named `__result`.
			// var __result: T = <zero>;
			const SymID result_symbol = ctx.query<QueryGeneratedSymbol>({
				.name                  = base::StrID("__result"),
				.generated_symbol_data = GeneratedSymbolData{ Variable{
					.function_symbol = copy_sym, .variable_index = 0, .type = result_symbol_type } },
			});
			body.emplace_back(makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				makeBox<code::DefaultValueExpr>(
					ctx, code::generatedOrigin(), result_symbol_type.getType()
				),
				result_symbol_type,
				result_symbol
			));

			// __result.field = <copy of (*source).field>;
			for (const auto& field: fields) {
				auto field_copy = makeCopyExpr(
					ctx, derefSourceField(ctx, source_symbol, field.getSymbol()), field.getType(ctx)
				);
				body.emplace_back(makeBox<code::AssignmentStmt>(
					code::generatedOrigin(),
					makeBox<code::AccessExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_symbol),
						field.getSymbol()
					),
					std::move(field_copy)
				));
			}

			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_symbol)
			));

			return body;
		}

		std::vector<Box<code::Stmt>> buildStaticArrayCopyBody(
			query::Context&                     ctx,
			const tsh::StaticArrayAbstractType& array_type,
			const SymID                         copy_sym,
			const SymID                         source_symbol,
			const tsh::SymbolType<>&            result_symbol_type
		) {
			using Variable = GeneratedSymbolData::GeneratedFunctionVariable;

			const auto  element_type = array_type.getElementType();
			const usize size         = array_type.getSize();

			std::vector<Box<code::Stmt>> body;

			// var __result: T[N] = <zero>;
			const SymID res_sym = ctx.query<QueryGeneratedSymbol>({
				.name                  = base::StrID("__result"),
				.generated_symbol_data = GeneratedSymbolData{ Variable{
					.function_symbol = copy_sym, .variable_index = 0, .type = result_symbol_type } },
			});
			body.emplace_back(makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				makeBox<code::DefaultValueExpr>(
					ctx, code::generatedOrigin(), result_symbol_type.getType()
				),
				result_symbol_type,
				res_sym
			));

			// Generate the copy loop only if the static array is not empty.
			if (size > 0) {
				const auto u64_abs_type
					= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned);
				const auto u64_type = tsh::SymbolType<>{ u64_abs_type,
					                                     tsh::ReferenceKind::Direct,
					                                     tsh::Mutability::Mutable };

				// var __i: u64 = 0;
				const SymID i_sym    = ctx.query<QueryGeneratedSymbol>({
					   .name                  = base::StrID("__i"),
					   .generated_symbol_data = GeneratedSymbolData{ Variable{
						   .function_symbol = copy_sym, .variable_index = 1, .type = u64_type } },
                });
				auto        zero_val = numeric_value::NumericValue::createOfType(u64_abs_type)
				                    .expect("u64 creation failed");
				body.emplace_back(makeBox<code::VariableStmt>(
					code::generatedOrigin(),
					makeBox<code::LiteralNumericExpr>(ctx, code::generatedOrigin(), zero_val),
					u64_type,
					i_sym
				));

				// while (__i < size) {
				// 		__result[__i] = <copy of (*source)[__i]>;
				// 		__i = __i + 1;
				// }
				auto size_val = numeric_value::NumericValue::createOfType(u64_abs_type, size)
				                    .expect("u64 creation failed");
				auto condition = makeBox<code::BinaryOperatorExpr>(
					ctx,
					code::generatedOrigin(),
					code::BuiltinBinary::IntegerLt,
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym),
					makeBox<code::LiteralNumericExpr>(ctx, code::generatedOrigin(), size_val)
				);

				code::CodeBlock loop_body{};

				// __result[__i] = <copy of (*source)[__i]>;
				auto source_element = makeBox<code::IndexExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::DerefExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), source_symbol)
					),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym)
				);
				loop_body.statements.emplace_back(makeBox<code::AssignmentStmt>(
					code::generatedOrigin(),
					makeBox<code::IndexExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), res_sym),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym)
					),
					makeCopyExpr(ctx, std::move(source_element), element_type)
				));

				// __i = __i + 1;
				auto one_val = numeric_value::NumericValue::createOfType(u64_abs_type, 1)
				                   .expect("u64 creation failed");
				loop_body.statements.emplace_back(makeBox<code::AssignmentStmt>(
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym),
					makeBox<code::BinaryOperatorExpr>(
						ctx,
						code::generatedOrigin(),
						code::BuiltinBinary::IntegerAdd,
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym),
						makeBox<code::LiteralNumericExpr>(ctx, code::generatedOrigin(), one_val)
					)
				));

				body.emplace_back(makeBox<code::WhileStmt>(
					code::generatedOrigin(), std::move(condition), std::move(loop_body)
				));
			}

			// return __result;
			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), res_sym)
			));

			return body;
		}

		std::vector<Box<code::Stmt>> buildDynamicArrayCopyBody(
			query::Context&                      ctx,
			const tsh::DynamicArrayAbstractType& array_type,
			const SymID                          copy_sym,
			const SymID                          source_symbol,
			const tsh::SymbolType<>&             result_symbol_type
		) {
			using Variable = GeneratedSymbolData::GeneratedFunctionVariable;

			const auto element_type = array_type.getElementType();

			std::vector<Box<code::Stmt>> body;

			// var __result: List[T] = <zero>;
			const SymID res_sym = ctx.query<QueryGeneratedSymbol>({
				.name                  = base::StrID("__result"),
				.generated_symbol_data = GeneratedSymbolData{ Variable{
					.function_symbol = copy_sym, .variable_index = 0, .type = result_symbol_type } },
			});
			body.emplace_back(makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				makeBox<code::DefaultValueExpr>(
					ctx, code::generatedOrigin(), result_symbol_type.getType()
				),
				result_symbol_type,
				res_sym
			));

			const auto u64_abs_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned);
			const auto u64_type = tsh::SymbolType<>{ u64_abs_type,
				                                     tsh::ReferenceKind::Direct,
				                                     tsh::Mutability::Mutable };

			// var __i: u64 = 0;
			const SymID i_sym    = ctx.query<QueryGeneratedSymbol>({
				   .name                  = base::StrID("__i"),
				   .generated_symbol_data = GeneratedSymbolData{ Variable{
					   .function_symbol = copy_sym, .variable_index = 1, .type = u64_type } },
            });
			auto        zero_val = numeric_value::NumericValue::createOfType(u64_abs_type)
			                    .expect("u64 creation failed");
			body.emplace_back(makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				makeBox<code::LiteralNumericExpr>(ctx, code::generatedOrigin(), zero_val),
				u64_type,
				i_sym
			));


			// while (__i < source.length()) {
			// 		__result[__i] = <copy of (*source)[__i]>;
			// 		__i = __i + 1;
			// }
			SymID length_method_sym = defgen::lengthMethodForType(ctx, array_type);

			std::vector<Box<code::Expr>> length_args;
			length_args.emplace_back(
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), source_symbol)
			);

			Box<code::Expr> len_expr = makeBox<code::CallExpr>(
				ctx,
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), length_method_sym),
				std::move(length_args)
			);

			auto condition = makeBox<code::BinaryOperatorExpr>(
				ctx,
				code::generatedOrigin(),
				code::BuiltinBinary::IntegerLt,
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym),
				std::move(len_expr)
			);

			code::CodeBlock loop_body{};

			// __result[__i] = <copy of (*source)[__i]>;
			auto source_element = makeBox<code::IndexExpr>(
				ctx,
				code::generatedOrigin(),
				makeBox<code::DerefExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), source_symbol)
				),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym)
			);
			loop_body.statements.emplace_back(makeBox<code::ExprStmt>(
				code::generatedOrigin(),
				makeBox<code::ListPushExpr>(
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), res_sym),
					makeCopyExpr(ctx, std::move(source_element), element_type)
				)
			));

			// __i = __i + 1;
			auto one_val = numeric_value::NumericValue::createOfType(u64_abs_type, 1)
			                   .expect("u64 creation failed");
			loop_body.statements.emplace_back(makeBox<code::AssignmentStmt>(
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym),
				makeBox<code::BinaryOperatorExpr>(
					ctx,
					code::generatedOrigin(),
					code::BuiltinBinary::IntegerAdd,
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym),
					makeBox<code::LiteralNumericExpr>(ctx, code::generatedOrigin(), one_val)
				)
			));

			body.emplace_back(makeBox<code::WhileStmt>(
				code::generatedOrigin(), std::move(condition), std::move(loop_body)
			));

			// return __result;
			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), res_sym)
			));

			return body;
		}
	}

	struct IMPLEMENT_QUERY(QueryDefaultCopyConstructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey owner_type) {
			const SymID copy_sym           = copySymForType(ctx, owner_type);
			const auto& cctor_decl         = ctx.query<QueryDeclOfFun>(copy_sym)->valueOrThrow();
			const SymID source_symbol      = cctor_decl.parameters.at(0).helios_symbol;
			const auto  result_symbol_type = cctor_decl.return_type;

			std::cout << "default copy ctor\n";

			std::vector<Box<code::Stmt>> body;
			switch (owner_type.getKind()) {
			case tsh::Kind::Class:
			case tsh::Kind::Tuple:
				body = buildAggregateCopyBody(
					ctx, owner_type, copy_sym, source_symbol, result_symbol_type
				);
				break;
			case tsh::Kind::StaticArray:
				body = buildStaticArrayCopyBody(
					ctx,
					owner_type.as<tsh::StaticArrayAbstractType>(),
					copy_sym,
					source_symbol,
					result_symbol_type
				);
				break;
			case tsh::Kind::DynamicArray:
				body = buildDynamicArrayCopyBody(
					ctx,
					owner_type.as<tsh::DynamicArrayAbstractType>(),
					copy_sym,
					source_symbol,
					result_symbol_type
				);
				break;
			// @TODO: #2000 Boxes (deep copy) and other non-aggregate non-trivially-copyable types.
			default:
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"Default copy constructor for type `", owner_type.toString(), "`."
					),
					""
				));
				return query::Failed();
			}

			return HOUTFunction(
				code::generatedOrigin(),
				&cctor_decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDefaultCopyConstructor);
}

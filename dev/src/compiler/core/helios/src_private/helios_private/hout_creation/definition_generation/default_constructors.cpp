#include "default_constructors.hpp"

#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios::houtgen {
	// -----------------------------------------------------------
	//                  Class Default Constructors
	// -----------------------------------------------------------
	struct IMPLEMENT_QUERY(QueryDefaultClassConstructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey class_type) {
			// Preamble, get some basic data.
			const SymID class_symbol    = class_type.getSymbol();
			auto        class_interface = class_type.getInterface(ctx);

			using DefaultClassConstructor = GeneratedSymbolData::DefaultClassConstructor;
			using Variable                = GeneratedSymbolData::Variable;
			using std::ranges::to;
			using std::views::transform;

			// Construct the constructor's type.
			// @TODO: #1328 Properly handle value categories in class constructors.
			const std::vector<tsh::InterfaceElement> fields
				= class_interface->getFieldsView() | to<std::vector>();

			// Prepare the ctor symbol and declaration.
			const SymID ctor_symbol = ctx.query<QueryGeneratedSymbol>(
				{ .name = name(class_symbol),
			      .generated_symbol_data
			      = GeneratedSymbolData{ DefaultClassConstructor{ class_symbol } } }
			);


			const auto& ctor_decl = ctx.query<QueryDeclOfFun>(ctor_symbol)->valueOrThrow();

			// Prepare the body of the constructor.
			std::vector<Box<code::Stmt>> body{};
			// - One declaration, one assignment per field, one return.
			body.reserve(1 + fields.size() + 1);

			// @TODO: #2307 Classes with a field named `__result`.
			// - Declare result variable.
			const auto  result_symbol_type = ctor_decl.return_type;
			const SymID result_symbol      = ctx.query<QueryGeneratedSymbol>({
					 .name = base::StrID("__result"),
					 .generated_symbol_data
                = GeneratedSymbolData{ Variable{ ctor_symbol, 0, result_symbol_type } },
            });

			// By default all fields with no initial value are zeroed.
			// var result: Class = <default_initializer>;
			body.emplace_back(makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				makeBox<code::DefaultValueExpr>(
					ctx, code::generatedOrigin(), result_symbol_type.getType()
				),
				result_symbol_type,
				result_symbol
			));

			// - Assign each field with the initializing expression or a default value expression.
			for (const auto& field: fields) {
				const auto field_pst_data = symbolPst(field.getSymbol())
				                                .value()
				                                .unlock(ctx)
				                                .dynamicCast<pst::Field>()
				                                .value();
				auto field_init_expr_opt = field_pst_data->getInit();

				auto init_expr = [&]() -> Box<code::Expr> {
					match_optional(field_init_expr_opt) {
						opt_some(field_init) {
							// If the field has an initializer value we use it.
							const auto field_type = field.getType(ctx);
							auto       expr
								= getHoutOfExprWithExpectedType(
									  ctx, field_init.unlock(ctx)->getExpr().unlock(ctx), field_type
								)
							          .valueOrThrow();
							return expr;
						}
						opt_none {
							// Otherwise initialize it with the default initializer expression.
							return ctx.query<QueryDefaultInitializerExpr>(field.getType(ctx))
							    ->valueOrThrow()
							    ->clone();
						}
					}
					CORE_UNREACHABLE();
				}();

				body.emplace_back(makeBox<code::AssignmentStmt>(
					code::generatedOrigin(),
					makeBox<code::AccessExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_symbol),
						field.getSymbol()
					),
					std::move(init_expr)
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

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDefaultClassConstructor);

	// -----------------------------------------------------------
	//              Static Array Default Constructors
	// -----------------------------------------------------------
	struct IMPLEMENT_QUERY(QueryDefaultStaticArrayConstructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey array_type) {
			// Preamble, get some basic data.
			const auto  element_type   = array_type.getElementType();
			const usize size           = array_type.getSize();
			const auto  array_sym_type = tsh::SymbolType<>{ array_type,
				                                            tsh::ReferenceKind::Direct,
				                                            tsh::Mutability::Mutable };

			using DefaultStaticArrayConstructor
				= GeneratedSymbolData::DefaultStaticArrayConstructor;
			using Variable = GeneratedSymbolData::Variable;

			// Prepare the ctor symbol and declaration.
			const SymID ctor_symbol = ctx.query<QueryGeneratedSymbol>({
				.name = base::StrID("__init_array"),
				.generated_symbol_data
				= GeneratedSymbolData{ DefaultStaticArrayConstructor{ array_type } },
			});

			const auto& ctor_decl = ctx.query<QueryDeclOfFun>(ctor_symbol)->valueOrThrow();

			std::vector<Box<code::Stmt>> body{};

			// var res: T[N];
			const SymID res_sym = ctx.query<QueryGeneratedSymbol>(
				{ .name = base::StrID("__result"),
			      .generated_symbol_data
			      = GeneratedSymbolData{ Variable{ ctor_symbol, 0, array_sym_type } } }
			);
			body.emplace_back(makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				makeBox<code::DefaultValueExpr>(ctx, code::generatedOrigin(), array_type),
				array_sym_type,
				res_sym
			));

			// Generate the loop only if the static array is not empty.
			if (size > 0) {
				auto u64_abs_type
					= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned);
				auto u64_type = tsh::SymbolType<>{ u64_abs_type,
					                               tsh::ReferenceKind::Direct,
					                               tsh::Mutability::Mutable };
				// var i: i64 = 0;
				const SymID i_sym = ctx.query<QueryGeneratedSymbol>(
					{ .name = base::StrID("__i"),
				      .generated_symbol_data
				      = GeneratedSymbolData{ Variable{ ctor_symbol, 1, u64_type } } }
				);
				auto zero_val = numeric_value::NumericValue::createOfType(u64_abs_type)
				                    .expect("u64 creation failed");
				body.emplace_back(makeBox<code::VariableStmt>(
					code::generatedOrigin(),
					makeBox<code::LiteralNumericExpr>(ctx, code::generatedOrigin(), zero_val),
					u64_type,
					i_sym
				));

				// i < size
				auto size_val = numeric_value::NumericValue::createOfType(u64_abs_type, size)
				                    .expect("u64 creation failed");
				auto condition = makeBox<code::BinaryOperatorExpr>(
					ctx,
					code::generatedOrigin(),
					code::BuiltinBinary::IntegerLt,
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym),
					makeBox<code::LiteralNumericExpr>(ctx, code::generatedOrigin(), size_val)
				);

				// while (i < size) { res[i] = default_init(T); i = i + 1; }
				code::CodeBlock loop_body{};
				auto            element_init
					= ctx.query<QueryDefaultInitializerExpr>(element_type)->valueOrThrow()->clone();

				// res[i] = default_init(T)
				loop_body.statements.emplace_back(makeBox<code::AssignmentStmt>(
					code::generatedOrigin(),
					makeBox<code::IndexExpr>(
						ctx,
						code::generatedOrigin(),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), res_sym),
						makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), i_sym)
					),
					element_init->clone()
				));

				// i = i + 1
				auto one_val = numeric_value::NumericValue::createOfType(u64_type.getType(), 1)
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

			// return result
			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), res_sym)
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

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDefaultStaticArrayConstructor);

	// -----------------------------------------------------------
	//                      TOP LEVEL QUERY
	// -----------------------------------------------------------
	struct IMPLEMENT_QUERY(QueryDefaultInitializerExpr, query::QResult<Box<code::Expr>>) {
		static PResult provide(Context& ctx, const QKey sym_type) {
			CORE_ASSERT(
				sym_type.isDefaultConstructible(ctx),
				"QueryDefaultInitializerExpr called on non default constructible type"
			);
			// For types that are trivially zero initializable, we just insert a default value expr
			// which will map to ZeroInitialize.
			if (sym_type.isTriviallyZeroInitializable(ctx)) {
				return makeBox<code::DefaultValueExpr>(
					ctx, code::generatedOrigin(), sym_type.getType()
				);
			}

			const auto& type = sym_type.getType();
			switch (type.getKind()) {
			case tsh::Kind::StaticArray: {
				auto array_type = type.as<tsh::StaticArrayAbstractType>();
				auto ctor
					= ctx.query<QueryDefaultStaticArrayConstructor>(array_type)->valueOrThrow();
				return makeBox<code::CallExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(
						ctx, code::generatedOrigin(), ctor.declaration->original_symbol
					),
					std::vector<Box<code::Expr>>{}
				);
			}
			case tsh::Kind::Class: {
				auto class_type = type.as<tsh::ClassAbstractType>();
				auto ctor = ctx.query<QueryDefaultClassConstructor>(class_type)->valueOrThrow();
				return makeBox<code::CallExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(
						ctx, code::generatedOrigin(), ctor.declaration->original_symbol
					),
					std::vector<Box<code::Expr>>{}
				);
			}
			case tsh::Kind::Tuple: {
				// @TODO: #2319 Add them here.
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					"Generating default constructors for not trivially zero-initializable "
					"tuple types.",
					std::nullopt
				));
				return query::Failed();
			}
			case tsh::Kind::Unit: {
				// Unit is default constructed with a unit.
				return makeBox<code::LiteralUnitExpr>(ctx, code::generatedOrigin());
			}
			case tsh::Kind::Meta: {
				// Meta is default initialized with a void type.
				return makeBox<code::LiteralTypeExpr>(
					ctx, code::generatedOrigin(), tsh::getVoidType()
				);
			}
			default: {
				CORE_PANIC(
					"Inconsistency between isTriviallyZeroInitializable and "
					"QueryDefaultInitializerExpr"
				);
			}
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDefaultInitializerExpr);

	query::QResult<Box<code::Expr>> getDefaultInitializerExpr(
		query::Context& ctx, const tsh::SymbolType<>& type, dia::SourcePosition pos
	) {
		if (!type.isDefaultConstructible(ctx)) {
			ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
				base::strConcat("Type `", type.toString(), "` cannot be default initialized"), pos
			));
			return query::Failed();
		}

		auto res = ctx.query<QueryDefaultInitializerExpr>(type);
		if (res->hasFailed()) return query::Failed();
		return res->valueOrThrow()->clone();
	}


}

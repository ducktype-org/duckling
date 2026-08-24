#include "default_constructors.hpp"

#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	using namespace code::shorthands;

	// -----------------------------------------------------------
	//                  Class Default Constructors
	// -----------------------------------------------------------
	struct IMPLEMENT_QUERY(QueryDefaultClassConstructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey class_type) {
			// Preamble, get some basic data.
			const SymID class_symbol    = class_type.getSymbol();
			auto        class_interface = class_type.getInterface(ctx);

			using std::ranges::to;
			using std::views::transform;

			// Construct the constructor's type.
			// @TODO: #1328 Properly handle value categories in class constructors.
			const std::vector<tsh::InterfaceElement> fields
				= class_interface->getFieldsView() | to<std::vector>();

			// Prepare the ctor symbol and declaration.
			const SymID ctor_symbol = ctx.query<QueryGeneratedSymbol>({
				.name = name(class_symbol),
				.generated_symbol_data
				= Constructor{ .type = class_type, .kind = Constructor::Kind::Default },
			});


			const auto& ctor_decl = ctx.query<QueryDeclOfFun>(ctor_symbol)->valueOrThrow();

			// Prepare the body of the constructor.
			const Shorthand s{ ctx };

			// One value per field, in declaration order: the field's own initializer when it has
			// one, its type's default initializer otherwise.
			std::vector<Box<code::Expr>> field_values;
			field_values.reserve(fields.size());

			for (const auto& field: fields) {
				const auto field_pst_data = maybeSymbolPst(field.getSymbol())
				                                .value()
				                                .unlock(ctx)
				                                .dynamicCast<pst::Field>()
				                                .value();
				auto field_init_expr_opt = field_pst_data->getInit();

				match_optional(field_init_expr_opt) {
					opt_some(field_init) {
						// If the field has an initializer value we use it. The HOUT of the
						// initializer may be owned by a query cache, in which case it is cloned.
						const auto field_type = field.getType(ctx);
						auto       field_init_hout
							= getHoutOfExprWithExpectedType(
								  ctx, field_init.unlock(ctx)->getExpr().unlock(ctx), field_type
							)
						          .valueOrThrow();
						field_values.emplace_back(
							field_init_hout.isBox() ? std::move(field_init_hout.getBox())
													: field_init_hout->clone()
						);
					}
					opt_none {
						// Otherwise initialize it with the default initializer expression. That
						// expr is owned by its query cache, so it has to be cloned into the
						// aggregate.
						field_values.emplace_back(
							ctx.query<QueryDefaultInitializerExpr>(field.getType(ctx))
								->valueOrThrow()
								->clone()
						);
					}
				}
			}

			// The whole value is built in place by a single expression:
			// `return create_aggregate(Class) { field_values... };`
			std::vector<Box<code::Stmt>> body{};
			body.emplace_back(s.ret(s.createAggregate(class_type, std::move(field_values))));

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
	//              Tuple Default Constructors
	// -----------------------------------------------------------

	struct IMPLEMENT_QUERY(QueryDefaultTupleConstructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey tuple_type) {
			// Preamble, get some basic data.
			auto tuple_interface = tuple_type.getInterface(ctx);

			const std::vector<tsh::InterfaceElement> fields
				= tuple_interface->getFieldsView() | std::ranges::to<std::vector>();

			// Prepare the ctor symbol and declaration.
			const SymID ctor_symbol = ctx.query<QueryGeneratedSymbol>({
				.name = base::StrID("__init_tuple"),
				.generated_symbol_data
				= Constructor{ .type = tuple_type, .kind = Constructor::Kind::Default },
			});


			const auto& ctor_decl = ctx.query<QueryDeclOfFun>(ctor_symbol)->valueOrThrow();

			// Prepare the body of the constructor.
			const Shorthand s{ ctx };

			// One value per field, in declaration order. Tuple fields have no initializers, so
			// every value is the field type's default initializer. Those exprs are owned by their
			// query cache, so they have to be cloned into the aggregate.
			std::vector<Box<code::Expr>> field_values;
			field_values.reserve(fields.size());
			for (const auto& field: fields)
				field_values.emplace_back(ctx.query<QueryDefaultInitializerExpr>(field.getType(ctx))
				                              ->valueOrThrow()
				                              ->clone());

			// `return create_aggregate(Tuple) { field_values... };`
			std::vector<Box<code::Stmt>> body{};
			body.emplace_back(s.ret(s.createAggregate(tuple_type, std::move(field_values))));

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

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDefaultTupleConstructor);

	// -----------------------------------------------------------
	//              Static Array Default Constructors
	// -----------------------------------------------------------
	struct IMPLEMENT_QUERY(QueryDefaultStaticArrayConstructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey array_type) {
			// Preamble, get some basic data.
			const auto  element_type = array_type.getElementType();
			const usize size         = array_type.getSize();

			// Prepare the ctor symbol and declaration.
			const SymID ctor_symbol = ctx.query<QueryGeneratedSymbol>({
				.name = base::StrID("__init_array"),
				.generated_symbol_data
				= Constructor{ .type = array_type, .kind = Constructor::Kind::Default },
			});

			const auto& ctor_decl = ctx.query<QueryDeclOfFun>(ctor_symbol)->valueOrThrow();

			const Shorthand s{ ctx };

			std::vector<Box<code::Stmt>> body{};

			if (size == 0) {
				// There is no element to construct, so there is nothing for an aggregate to build.
				// `return zero_initialized T[0];`
				body.emplace_back(s.ret(s.defaultValue(array_type)));
			} else {
				std::vector<Box<code::Expr>> element_values;
				element_values.emplace_back(
					ctx.query<QueryDefaultInitializerExpr>(element_type)->valueOrThrow()->clone()
				);

				// `return create_aggregate(T[N]) [ element_init ];`
				body.emplace_back(s.ret(s.createAggregate(array_type, std::move(element_values))));
			}

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
			const Shorthand s{ ctx };

			// For types that are trivially zero initializable, we just insert a default value expr
			// which will map to ZeroInitialize.
			if (sym_type.isTriviallyZeroInitializable(ctx))
				return s.defaultValue(sym_type.getType());

			const auto& type = sym_type.getType();
			switch (type.getKind()) {
			case tsh::Kind::StaticArray: {
				auto array_type = type.as<tsh::StaticArrayAbstractType>();
				auto ctor
					= ctx.query<QueryDefaultStaticArrayConstructor>(array_type)->valueOrThrow();
				return s.call(s.ident(ctor.declaration->original_symbol));
			}
			case tsh::Kind::Class: {
				auto class_type = type.as<tsh::ClassAbstractType>();
				auto ctor = ctx.query<QueryDefaultClassConstructor>(class_type)->valueOrThrow();
				return s.call(s.ident(ctor.declaration->original_symbol));
			}
			case tsh::Kind::Tuple: {
				auto tuple_type = type.as<tsh::TupleAbstractType>();
				auto ctor = ctx.query<QueryDefaultTupleConstructor>(tuple_type)->valueOrThrow();
				return s.call(s.ident(ctor.declaration->original_symbol));
			}
			case tsh::Kind::Unit: {
				// Unit is default constructed with a unit.
				return s.litUnit();
			}
			case tsh::Kind::Meta: {
				// Meta is default initialized with a void type.
				return s.litType(tsh::getVoidType());
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

	query::QResult<CRef<code::Expr>> getDefaultInitializerExpr(
		query::Context& ctx, const tsh::SymbolType<>& type, dia::StablePosition pos
	) {
		if (!type.isDefaultConstructible(ctx)) {
			ctx.logInt(makeBox<dia::PlaceholderError>(
				base::strConcat("Type `", type.toString(), "` cannot be default initialized"), pos
			));
			return query::Failed();
		}

		auto res = ctx.query<QueryDefaultInitializerExpr>(type);
		if (res->hasFailed()) return query::Failed();
		return res->valueOrThrow().ref();
	}


}

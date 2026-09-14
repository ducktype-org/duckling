#include "global_data_queries.hpp"

#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/variable.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	struct IMPLEMENT_QUERY(QueryHOUTGlobalData, query::QResult<HOUTGlobalData>) {
		/**
		 * @brief Whether the field symbol is a static one, that is one that is stored once for
		 * the whole program instead of once per instance of its class.
		 */
		static bool isStaticField(Context& ctx, SymID symbol) {
			auto element = typeMemberOwner(symbol).getInterface(ctx)->getElementBySym(symbol);
			return element.has_value() and element.value()->isStaticField();
		}

		/**
		 * @brief The parts of a declaration of a global that its HOUT is built from.
		 *
		 * A global is declared either as a variable or as a static field of a class, and both are
		 * built the same way, only their PST elements differ.
		 */
		struct GlobalDeclaration final {
			/**
			 * @brief The initializing value of the declaration, if it has one.
			 */
			base::Optional<pst::AccessLocked<pst::ExprHolder>> initial_value;

			/**
			 * @brief Position of the declared name, reported for a bad initializing value.
			 */
			dia::StablePosition name_position;

			/**
			 * @brief Position of the whole declaration, reported for a type that cannot be
			 * default initialized.
			 */
			dia::StablePosition position;
		};

		static GlobalDeclaration globalDeclarationOf(Context& ctx, SymID symbol) {
			auto pst_declaration = stmt(ctx, symbol).value();

			if (auto variable = pst_declaration.dynamicCast<pst::Variable>())
				return { .initial_value = variable.value()->getValue(),
					     .name_position
					     = variable.value()->getName().unlock(ctx)->getStablePosition(),
					     .position = variable.value()->getStablePosition() };

			if (auto field = pst_declaration.dynamicCast<pst::Field>())
				return { .initial_value = field.value()->getInit(),
					     .name_position = field.value()->getName().unlock(ctx)->getStablePosition(),
					     .position      = field.value()->getStablePosition() };

			CORE_PANIC("A global is declared either as a variable or as a static field.");
		}

		static auto provide(Context& ctx, QKey symbol) -> PResult {
			auto symbol_kind = kind(symbol);

			// The empty storage of a REPL global variable shares everything with the variable it
			// stands for, except for the initial value. It shares the same symbol as for the
			// original variable, to have the same mangling.
			if (auto empty_variable
			    = std::get_if<defgen::ReplEmptyVariable>(&getSymRef(symbol)->other)) {
				const auto& original
					= ctx.query<QueryHOUTGlobalData>(empty_variable->original_variable)
				          ->valueOrThrow();
				CORE_ASSERT(
					original.data_type == HOUTGlobalDataType::Variable,
					"An empty REPL variable expects a variable declaration"
				);

				Box<code::Expr> empty_value = base::makeBox<code::DefaultValueExpr>(
					ctx, code::generatedOrigin(), original.type.getType()
				);

				return HOUTGlobalData{
					.helios_symbol = symbol,
					.origin        = original.origin,
					.original_name = original.original_name,
					.data_type     = original.data_type,
					.value         = HOUTGlobalVariable{ std::move(empty_value) },
					.type          = original.type,
				};
			}

			CORE_ASSERT(
				symbol_kind == SymbolKind::Const
					|| (symbol_kind == SymbolKind::Variable && isGlobalVar(ctx, symbol))
					|| (symbol_kind == SymbolKind::Field && isStaticField(ctx, symbol)),
				"QueryHOUTGlobalData expects a global const, a global variable or a static field"
			);

			// A static field is stored the same way a global variable is, as the only data that
			// lives in the program instead of in an instance of its class.
			auto data_type = symbol_kind == SymbolKind::Const ? HOUTGlobalDataType::Constant
			                                                  : HOUTGlobalDataType::Variable;

			const auto symbol_type = ctx.query<QueryTypeOfSymbol>(symbol)->valueOrThrow();

			auto maybe_pst_decl = maybeSymbolPst(symbol);
			auto origin         = [&]() -> code::ElementOrigin {
                if (maybe_pst_decl.has_value()) {
                    auto pst_decl = maybe_pst_decl.value().unlock(ctx);
                    return code::pstOrigin(pst_decl);
                } else {
                    // Note: we could generate better origin upon const creation and use it here,
                    // if we ever needed to
                    return code::generatedOrigin();
                }
			}();

			auto value = [&]() -> std::variant<HOUTGlobalConst, HOUTGlobalVariable> {
				switch (data_type) {
				case HOUTGlobalDataType::Variable: {
					CORE_ASSERT(
						maybe_pst_decl.has_value(),
						"Global variable symbol without PST Implemented Semantics is not handled "
						"in QueryHOUTGlobalData"
					);

					auto declaration = globalDeclarationOf(ctx, symbol);

					auto get_initial_value = [&]() -> BoxOrCRef<code::Expr> {
						if_opt_some(declaration.initial_value, initial_value_holder) {
							auto initial_value_pst = initial_value_holder.unlock(ctx)->getExpr();
							return getHoutOfExprWithExpectedType(
									   ctx, initial_value_pst, symbol_type, declaration.name_position
							)
							    .valueOrThrow();
						}

						return defgen::getDefaultInitializerExpr(
								   ctx, symbol_type, declaration.position
						)
						    .valueOrThrow();
					};
					auto initial_value = get_initial_value();

					return HOUTGlobalVariable{ std::move(initial_value) };
				}
				case HOUTGlobalDataType::Constant:
					return HOUTGlobalConst{ ctx.query<QueryConstValueOf>(symbol).valueOrThrow() };
				default:
					CORE_PANIC("Unhandled HOUTGlobalDataType");
				}
			}();

			return HOUTGlobalData{
				.helios_symbol = symbol,
				.origin        = origin,
				.original_name = name(symbol),
				.data_type     = data_type,
				.value         = std::move(value),
				.type          = symbol_type,
			};
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHOUTGlobalData);

	Box<code::Expr> getGlobalConstructorExpr(query::Context& ctx, CRef<HOUTGlobalData> global_data) {
		using namespace code::shorthands;
		Shorthand s(ctx);

		CORE_ASSERT(
			v_matches(global_data->value, HOUTGlobalVariable),
			"Call only valid with global variable."
		);

		CRef<code::Expr> initial_value_expr
			= v_get(global_data->value, HOUTGlobalVariable).initial_value.ref();

		auto move_in_symbol = moveInSymForType(ctx, global_data->type);
		return s.call(
			s.ident(move_in_symbol),
			s.ptrOf(s.ident(global_data->helios_symbol)),
			initial_value_expr->clone()
		);
	}

	base::Optional<Box<code::Expr>> getGlobalDestructorExpr(
		query::Context& ctx, CRef<HOUTGlobalData> global_data
	) {
		using namespace code::shorthands;
		Shorthand s(ctx);
		auto      destructor = getTypeDestructor(ctx, global_data->type);
		if (destructor.empty()) return {};

		// refOf here is intentional, for `box T` reference types
		// we change the type to `ref T` and the MIR will remove the additional address of.
		// And for direct types it will just add the address of.
		return s.call(s.ident(destructor.value()), s.refOf(s.ident(global_data->helios_symbol)));
	}
}

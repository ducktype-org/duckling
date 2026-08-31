#include "square_call_processing.hpp"

#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/nested_import_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>
#include <helios_private/hout_creation/expressions/hout_of_subexpr.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>

#include <diagnostic/placeholder.hpp>

namespace compiler::helios::code {
	namespace {
		/**
		 * @brief Processes an array indexing operation `[ix]` on an expression.
		 *
		 * - The `[ix]` expects the base to be an array-like type and a direct type (inserts
		 * DerefExpr if needed).
		 * - The argument for the operator has to be implicitly coercible to `i64`.
		 *
		 * @param ctx The query context.
		 * @param current_expr The base expression to be indexed.
		 * @param index_pst The PST node for the index expression.
		 * @return A query result containing the `IndexExpr` on success, or a failed result on error.
		 */
		query::QResult<base::Box<Expr>> processArrayIndexing(
			query::Context&                     ctx,
			base::Box<Expr>                     current_expr,
			pst::AccessLocked<pst::ExprElement> index_pst
		) {
			if (auto expr_kind = current_expr->expression_type.getSymbolType().getType().getKind();
			    not tsh::isIndexable(expr_kind)) {
				auto error_pos = current_expr->origin.getStablePosition().copyValueOr(
					index_pst.unlock(ctx)->getStablePosition()
				);
				ctx.logInt(makeBox<dia::PlaceholderError>(
					"Index operator base must be indexable.", error_pos
				));
				return query::Failed();
			}

			// Base has to be direct for array access.
			if (current_expr->expression_type.getSymbolType().getRefKind()
			    != tsh::ReferenceKind::Direct)
				current_expr = makeBox<DerefExpr>(
					ctx, current_expr->origin.generatedFrom(), std::move(current_expr)
				);

			// @TODO: #1532 This i64 coercion should be handled by the `[]` operator.
			// @TODO: #2754 Possibly adjust the type based on the actual type of the index expression.
			auto i64_type = tsh::SymbolType<>{
				tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Immutable
			};
			auto index_res = subExprFromPSTWithType(ctx, index_pst, i64_type);
			UNPACK_QRESULT_MOVE(base::Box<Expr> index_expr =, index_res);

			return makeBox<IndexExpr>(
				ctx,
				pstOriginOrdered(current_expr->origin, index_pst.unlock(ctx)),
				std::move(current_expr),
				std::move(index_expr)
			);
		}

		/**
		 * @brief Processes the creation of array-related types. This includes:
		 * - Type template baking - if the base is a 'TypeTemplate' type (e.g., bare 'List'
		 * keyword), the index argument is expected to be Meta.
		 * - Static Array Type Creation - if the base is a meta type (e.g., 'i32'), the index
		 * argument is expected to be an integral constant representing the array size. This results
		 * in a 'StaticArray' type.
		 *
		 * @param base The base expression being indexed.
		 * @param arg_pst The PST element inside the square brackets.
		 * @return A ChainState containing an IndexExpr representing the type construction.
		 */
		query::QResult<base::Box<Expr>> processArrayTypeCreation(
			query::Context& ctx, Box<Expr> base, pst::AccessLocked<pst::ExprElement> arg_pst
		) {
			// If base is a type template, we expect meta in the index arguments for baking the
			// template type. Otherwise we expect an integer for StaticArray type creation.
			// @TODO: #1532 This u64/meta coercions should be handled by the `[]` operator.
			auto expected_index_arg_type = [&]() -> tsh::SymbolType<> {
				// @TODO: #1918 This logic should be generalized to handle any expressions with
				// TypeTemplate type, not just literals.
				if (auto* literal_type_expr = dynamic_cast<LiteralTypeExpr*>(base.get())) {
					if (literal_type_expr->value_type.getType().getKind()
					    == tsh::Kind::TypeTemplate) {
						return tsh::SymbolType<>{
							tsh::getMetaType(),

							tsh::ReferenceKind::Direct,
							tsh::Mutability::Immutable,
						};
					}
				}

				return tsh::SymbolType<>{
					// @TODO: #2754 Possibly adjust the type based on the actual type of the index
					// expression.
					tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable
				};
			}();

			auto arg_res = subExprFromPSTWithType(ctx, arg_pst, expected_index_arg_type);
			UNPACK_QRESULT_MOVE(Box<Expr> arg_expr =, arg_res);

			auto total_origin = pstOriginOrdered(base->origin, arg_pst.unlock(ctx));
			return makeBox<IndexExpr>(ctx, total_origin, std::move(base), std::move(arg_expr));
		}
	}

	query::QResult<base::Box<Expr>> processSquareCall(
		query::Context& ctx, base::Box<Expr> base, pst::Access<pst::expr::Call> call_expr
	) {
		CORE_ASSERT(
			call_expr->getType() == lexer::Token::Square,
			"processSquareCall called on a different call type: ",
			char(call_expr->getType())
		);

		// @TODO: #1532 This check should be handled by the `[]` operator.
		auto args = call_expr->getArgs().unlock(ctx);
		if (args->size() != 1) {
			ctx.logInt(makeBox<dia::PlaceholderError>(
				"Array index/size must be exactly one expression.", call_expr->getStablePosition()
			));
			return query::Failed();
		}

		auto arg_pst = (*args->begin()).unlock(ctx)->getArg().unlock(ctx)->getExpr();

		auto meta_res = canCoerceToMeta(ctx, base->expression_type);
		UNPACK_QRESULT_MOVE(auto meta_coercion_res =, meta_res);

		// If base is coercible to meta, this is an array type creation.
		if (meta_coercion_res.isValid()) {
			auto coerced_base = meta_coercion_res.coerce(ctx, std::move(base));
			return processArrayTypeCreation(ctx, std::move(coerced_base), arg_pst);
		}

		// Otherwise, it's an index operator.
		auto array_indexing_res = processArrayIndexing(ctx, std::move(base), arg_pst);
		UNPACK_QRESULT_MOVE(base::Box<Expr> result_expr =, array_indexing_res);
		return result_expr;
	}
}

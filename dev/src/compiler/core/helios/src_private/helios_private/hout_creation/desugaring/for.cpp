#include "for.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/kind.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/value_category.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/definition_generation/length_methods.hpp>
#include <helios_private/hout_creation/expressions/coercions.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/context/context.hpp>
#include <string_id/string_id.hpp>

namespace compiler::helios::desugaring {
	namespace {
		struct ForDesugarCtx {
			query::Context&       ctx;
			pst::Access<pst::For> stmt;

			code::ElementOrigin loop_origin;
			code::ElementOrigin iterable_origin;
			code::ElementOrigin iterator_origin;

			// Iterable info.
			tsh::SymbolType<> iterable_type;
			bool              collection_is_l_value;
			Box<code::Expr>   iterable_hout;
		};

		tsh::SymbolType<> getU64(query::Context& ctx) {
			return tsh::SymbolType<>::withDefaults(
				tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned)
			);
		}

		tsh::SymbolType<> getConstU64(query::Context& ctx) {
			return tsh::SymbolType<>::withDefaults(
					   tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned)
			)
			    .withMutability(tsh::Mutability::Immutable);
		}

		base::Optional<ForDesugarCtx> buildForDesugarCtx(
			query::Context& ctx, pst::Access<pst::For> stmt
		) {
			// Get the iterable and it's type.
			auto iterable_pst      = stmt->getIterable().unlock(ctx);
			auto iterable_hout_res = ctx.query<QueryHoutOfExpr>({ iterable_pst->getExpr() });
			if (iterable_hout_res->hasFailed()) return {};

			Box<code::Expr>   iterable_hout = iterable_hout_res->valueOrThrow()->clone();
			tsh::SymbolType<> iterable_type = iterable_hout->expression_type.getSymbolType();
			tsh::Kind         kind          = iterable_type.getType().getKind();
			bool              iterable_is_r_value
				= not iterable_hout->expression_type.getValueCategory().canBeAssignedTo();

			if (kind != tsh::Kind::DynamicArray && kind != tsh::Kind::StaticArray) {
				ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
					base::strConcat(
						"`for` statements for non-array type: ", iterable_type.toString()
					),
					code::pstOrigin(iterable_pst).getSourcePosition(ctx)
				));
				return {};
			}

			return ForDesugarCtx{
				.ctx                   = ctx,
				.stmt                  = stmt,
				.loop_origin           = code::pstOrigin(stmt),
				.iterable_origin       = code::pstOrigin(iterable_pst),
				.iterator_origin       = code::pstOrigin(stmt->getIteratorIdentifier().unlock(ctx)),
				.iterable_type         = iterable_type,
				.collection_is_l_value = not iterable_is_r_value,
				.iterable_hout         = std::move(iterable_hout),
			};
		}

		// var __idx: u64 = 0
		Box<code::Stmt> buildIndexVar(const ForDesugarCtx& ctx, SymID idx_sym) {
			return makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				makeBox<code::DefaultValueExpr>(
					ctx.ctx, code::generatedOrigin(), getU64(ctx.ctx).getType()
				),
				getU64(ctx.ctx),
				idx_sym
			);
		}

		// Length is calculated once before the loop.
		// let __len: u64 = <len __collection> / <constant>
		Box<code::Stmt> buildLengthVar(
			const ForDesugarCtx& ctx, SymID len_sym, Box<code::Expr> iterable_reusable_opt
		) {
			const auto gen = code::generatedOrigin();

			SymID length_method_sym
				= defgen::lengthMethodForType(ctx.ctx, ctx.iterable_type.getType());

			Box<code::Expr> length_arg = std::move(iterable_reusable_opt);
			if (not ctx.collection_is_l_value)
				length_arg = makeBox<code::RefOfExpr>(ctx.ctx, gen, std::move(length_arg));

			std::vector<Box<code::Expr>> length_args;
			length_args.emplace_back(std::move(length_arg));

			Box<code::Expr> len_expr = makeBox<code::CallExpr>(
				ctx.ctx,
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx.ctx, code::generatedOrigin(), length_method_sym),
				std::move(length_args)
			);

			return makeBox<code::VariableStmt>(
				gen, std::move(len_expr), getConstU64(ctx.ctx), len_sym
			);
		}

		// __idx < __len
		Box<code::Expr> buildCondition(const ForDesugarCtx& ctx, SymID idx_sym, SymID len_sym) {
			const auto gen = code::generatedOrigin();
			return makeBox<code::BinaryOperatorExpr>(
				ctx.ctx,
				gen,
				code::BuiltinBinary::IntegerLt,
				makeBox<code::IdentifierExpr>(ctx.ctx, gen, idx_sym),
				makeBox<code::IdentifierExpr>(ctx.ctx, gen, len_sym)
			);
		}

		// let <iter> = <__collection_next_use>[__idx];
		// <body>;
		// __idx = __idx + 1;
		base::Optional<code::CodeBlock> buildWhileBody(
			const ForDesugarCtx& ctx,
			Box<code::Expr>      collection_next_use,
			SymID                idx_sym,
			SymID                iter_sym,
			tsh::SymbolType<>    iter_type,
			const BodyProcessor& process_body
		) {
			const auto gen = code::generatedOrigin();
			auto idx_ref   = [&] { return makeBox<code::IdentifierExpr>(ctx.ctx, gen, idx_sym); };

			// If the iterable is not direct, we dereference it first.
			Box<code::Expr> base_expr = [&]() -> Box<code::Expr> {
				bool needs_deref = ctx.iterable_type.getRefKind() != tsh::ReferenceKind::Direct
				                || ctx.collection_is_l_value;
				if (needs_deref)
					return makeBox<code::DerefExpr>(ctx.ctx, gen, std::move(collection_next_use));
				return std::move(collection_next_use);
			}();


			// __collection[__idx]
			Box<code::Expr> raw_element
				= makeBox<code::IndexExpr>(ctx.ctx, gen, std::move(base_expr), idx_ref());

			auto iter_pst_pos     = ctx.stmt->getIterable().unlock(ctx.ctx)->getStablePosition();
			auto element_sym_type = raw_element->expression_type.getSymbolType();
			auto element_expr     = coerceFromBox(
                ctx.ctx,
                std::move(raw_element),
                iter_type,
                iter_pst_pos,
                {},
                CoercionErrorOverrides{
						.incompatible_types =
                        [&](query::Context& error_ctx) {
                            error_ctx.logInt(makeBox<dia::PlaceholderError>(
                                base::strConcat(
                                    "Cannot coerce collection element type '",
                                    element_sym_type.toString(),
                                    "' to iterator type '",
                                    iter_type.toString(),
                                    "'."
                                ),
                                iter_pst_pos
                            ));
                        },
                }
            );
			if (!element_expr.has_value()) return {};

			code::CodeBlock body{};

			// let <user_var> = __collection[__idx];
			body.statements.emplace_back(makeBox<code::VariableStmt>(
				ctx.iterator_origin, std::move(element_expr.value()), iter_type, iter_sym
			));

			// <body>;
			auto user_body = process_body(ctx.stmt->getBody());
			for (auto& s: user_body.statements) body.statements.emplace_back(std::move(s));

			// @TODO: #2465 When adding `continue` etc. remember to not jump over the increment line.
			// __idx = __idx + 1;
			auto one = makeBox<code::LiteralNumericExpr>(
				ctx.ctx,
				gen,
				compiler::numeric_value::NumericValue::createOfType<u64>(
					getU64(ctx.ctx).getType(), 1
				)
					.value()
			);
			body.statements.emplace_back(makeBox<code::AssignmentStmt>(
				gen,
				idx_ref(),
				makeBox<code::BinaryOperatorExpr>(
					ctx.ctx, gen, code::BuiltinBinary::IntegerAdd, idx_ref(), std::move(one)
				)
			));

			return body;
		}
	}

	ForGeneratedSymbols getForGeneratedSymbols(query::Context& ctx, pst::Access<pst::For> stmt) {
		ScopeID for_scope = ctx.query<QueryPrimaryCodeScopeFor>({ stmt });

		auto get_generated_local = [&](base::StrID role, tsh::SymbolType<> type) {
			// Compose the name with the current scope hash, so we don't have naming collisions
			// with nested loops.
			auto unique = for_scope.queryUnstablePerfectHash();
			auto name   = base::StrID(base::strConcat(role, unique));

			// @TODO: #2799 Reconsider the generated symbols scope.
			return ctx.query<defgen::QueryGeneratedSymbol>({
				.name = name,
				.generated_symbol_data
				= defgen::ControlFlowLocal{
					.owning_scope = for_scope,
					.role         = role,
					.type         = type,
				},
			});
		};

		// @TODO: #3290 generated variables names
		return {
			.iterator
			= ctx.query<QuerySymbolOfSTMT>({ stmt->getIteratorIdentifier() }).valueOrThrow(),
			.index  = get_generated_local(base::StrID("__index"), getU64(ctx)),
			.length = get_generated_local(base::StrID("__len"), getConstU64(ctx)),
		};
	}

	base::Optional<code::BlockStmt> desugarFor(
		query::Context& ctx, pst::Access<pst::For> stmt, const BodyProcessor& process_body
	) {
		auto gen     = code::generatedOrigin();
		auto ctx_opt = buildForDesugarCtx(ctx, stmt);
		if (!ctx_opt.has_value()) return {};
		auto& for_ctx = ctx_opt.value();

		// Get the needed symbols.
		ForGeneratedSymbols symbols = getForGeneratedSymbols(ctx, stmt);
		tsh::SymbolType<>   iter_type
			= ctx.query<QueryTypeOfSymbol>(symbols.iterator)->valueOrThrow();

		Box<code::Expr> reusable_inner = [&]() -> Box<code::Expr> {
			if (for_ctx.collection_is_l_value)
				return makeBox<code::RefOfExpr>(ctx, gen, std::move(for_ctx.iterable_hout));
			return std::move(for_ctx.iterable_hout);
		}();


		auto iterable_reusable = makeBox<code::ReusableExpr>(ctx, std::move(reusable_inner), true);
		auto iterable_next_use = iterable_reusable->nextUse();

		// Build the while body first so we bail out on coercion failure before
		// consuming the iterable expression into the ReusableExpr.
		auto while_body = buildWhileBody(
			for_ctx,
			std::move(iterable_next_use),
			symbols.index,
			symbols.iterator,
			iter_type,
			process_body
		);
		if (!while_body.has_value()) return {};

		// Desugar the loop.
		// var __index : u64 = 0u64;
		// var __len: const u64 = <constant> / len <iterable_reusable>;
		// while(__idx < __len) {
		// 		let <iter> = <iterable_reusable>[__idx];
		// 		<body>;
		// 		__idx = __idx + 1;
		// }
		code::CodeBlock outer{};
		outer.statements.emplace_back(buildIndexVar(for_ctx, symbols.index));
		outer.statements.emplace_back(
			buildLengthVar(for_ctx, symbols.length, std::move(iterable_reusable))
		);


		outer.statements.emplace_back(makeBox<code::WhileStmt>(
			for_ctx.loop_origin,
			buildCondition(for_ctx, symbols.index, symbols.length),
			std::move(while_body.value())
		));

		// Emit it in a block so variable names don't collide if two fors are in the same
		// function and for the collection scope to be not accessible from outside the for loop.
		return code::BlockStmt(for_ctx.loop_origin, std::move(outer));
	}
}

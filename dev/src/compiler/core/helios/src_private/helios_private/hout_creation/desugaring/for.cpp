#include "for.hpp"

#include <frontend/pst_parser/access.hpp>
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
			ScopeID               for_scope;

			code::ElementOrigin loop_origin;
			code::ElementOrigin iterable_origin;
			code::ElementOrigin iterator_origin;

			tsh::SymbolType<> u64_mut_type;
			tsh::SymbolType<> u64_immut_type;

			// Iterable info.
			tsh::SymbolType<> iterable_type;
			bool              collection_is_l_value;
			tsh::SymbolType<> col_type;
			Box<code::Expr>   iterable_hout;
		};

		base::Optional<ForDesugarCtx> buildForDesugarCtx(
			query::Context& ctx, pst::Access<pst::For> stmt
		) {
			// Get the iterable and it's type.
			auto iterable_pst      = stmt->getIterable().unlock(ctx);
			auto iterable_hout_res = ctx.query<QueryHoutOfExpr>({ iterable_pst->getExpr() });
			if (iterable_hout_res->hasFailed()) return {};

			Box<code::Expr>      iterable_hout = iterable_hout_res->valueOrThrow()->clone();
			tsh::SymbolType<>    iterable_type = iterable_hout->expression_type.getSymbolType();
			tsh::Kind            kind          = iterable_type.getType().getKind();
			tsh::PrimaryCategory value_category
				= iterable_hout->expression_type.getValueCategory().getCategory();
			bool iterable_is_r_value = value_category == tsh::PrimaryCategory::Literal
			                        || value_category == tsh::PrimaryCategory::Temporary;

			if (kind != tsh::Kind::DynamicArray && kind != tsh::Kind::StaticArray) {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"`for` statements for non-array type: ", iterable_type.toString()
					),
					stmt->getStablePosition()
				));
				return {};
			}

			auto u64_mut = tsh::SymbolType<>::withDefaults(
				tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned)
			);

			return ForDesugarCtx{
				.ctx                   = ctx,
				.stmt                  = stmt,
				.for_scope             = ctx.query<QueryPrimaryCodeScopeFor>({ stmt }),
				.loop_origin           = code::pstOrigin(stmt),
				.iterable_origin       = code::pstOrigin(iterable_pst),
				.iterator_origin       = code::pstOrigin(stmt->getIteratorIdentifier().unlock(ctx)),
				.u64_mut_type          = u64_mut,
				.u64_immut_type        = u64_mut.withMutability(tsh::Mutability::Immutable),
				.iterable_type         = iterable_type,
				.collection_is_l_value = not iterable_is_r_value,
				.col_type              = iterable_is_r_value
				                           ? iterable_type
				                           : iterable_type.withReferenceKind(tsh::ReferenceKind::Ref),
				.iterable_hout         = std::move(iterable_hout),
			};
		}

		SymID makeForLocal(const ForDesugarCtx& ctx, base::StrID role, tsh::SymbolType<> type) {
			// Compose the name with the current scope hash, so we don't have naming collisions
			// with nested loops.
			auto unique = ctx.for_scope.queryUnstablePerfectHash();
			auto name   = base::StrID(base::strConcat(role, unique));

			using GeneratedSymbolData = defgen::GeneratedSymbolData;
			using ControlFlowLocal    = GeneratedSymbolData::ControlFlowLocal;

			return ctx.ctx.query<defgen::QueryGeneratedSymbol>({
				.name                  = name,
				.generated_symbol_data = GeneratedSymbolData{ ControlFlowLocal{
					.owning_scope = ctx.for_scope,
					.role         = role,
					.type         = type,
				} },
			});
		}

		// Create a temp for the collection for it to be evaluated only once before the loop.
		// var __collection: ref T = &<iterable>      (l-value iterable)
		// var __collection: T     = <iterable>       (r-value iterable)
		Box<code::Stmt> buildCollectionVar(ForDesugarCtx& ctx, SymID col_sym) {
			Box<code::Expr> collection_expr = [&]() -> Box<code::Expr> {
				if (ctx.collection_is_l_value) {
					// If the collection if it's an l-value we operate on it through a ref.
					return makeBox<code::RefOfExpr>(
						ctx.ctx, code::generatedOrigin(), std::move(ctx.iterable_hout)
					);
				} else {
					// If the collection is a r-value we store it in the `__collection` variable
					// directly.
					return std::move(ctx.iterable_hout);
				}
			}();

			return makeBox<code::VariableStmt>(
				ctx.iterable_origin, std::move(collection_expr), ctx.col_type, col_sym
			);
		}

		// var __idx: u64 = 0
		Box<code::Stmt> buildIndexVar(const ForDesugarCtx& ctx, SymID idx_sym) {
			return makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				makeBox<code::DefaultValueExpr>(
					ctx.ctx, code::generatedOrigin(), ctx.u64_mut_type.getType()
				),
				ctx.u64_mut_type,
				idx_sym
			);
		}

		// Length is calculated once before the loop.
		// let __len: u64 = <len __collection> / <constant>
		Box<code::Stmt> buildLengthVar(const ForDesugarCtx& ctx, SymID len_sym, SymID col_sym) {
			const auto gen = code::generatedOrigin();

			auto len_expr = [&]() -> Box<code::Expr> {
				switch (ctx.iterable_type.getType().getKind()) {
				case tsh::Kind::DynamicArray: {
					auto collection_expr = [&]() -> Box<code::Expr> {
						if (ctx.collection_is_l_value) {
							// If collection was an l-value we have to dereference it since we
							// operate on it through a ref.
							return makeBox<code::DerefExpr>(
								ctx.ctx, gen, makeBox<code::IdentifierExpr>(ctx.ctx, gen, col_sym)
							);
						} else {
							return makeBox<code::IdentifierExpr>(ctx.ctx, gen, col_sym);
						}
					}();

					return makeBox<code::UnaryOperatorExpr>(
						ctx.ctx, gen, code::BuiltinUnary::Len, std::move(collection_expr)
					);
				}
				case tsh::Kind::StaticArray: {
					auto size
						= ctx.iterable_type.getType().as<tsh::StaticArrayAbstractType>().getSize();
					return makeBox<code::LiteralNumericExpr>(
						ctx.ctx,
						gen,
						compiler::numeric_value::NumericValue::createOfType<u64>(
							ctx.u64_immut_type.getType(), size
						)
							.value()
					);
				}
				default:
					CORE_UNREACHABLE();
				}
			}();

			return makeBox<code::VariableStmt>(
				ctx.loop_origin, std::move(len_expr), ctx.u64_immut_type, len_sym
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

		// Apply implicit coercion from the collection's element type to the
		// user-declared iterator type. Returns nullopt and logs an error if no
		// coercion exists.
		base::Optional<Box<code::Expr>> coerceElementToIter(
			const ForDesugarCtx& ctx, Box<code::Expr> element_expr, tsh::SymbolType<> iter_type
		) {
			auto element_sym_type = element_expr->expression_type.getSymbolType();
			auto coercion_res     = canCoerce(ctx.ctx, element_sym_type, iter_type);
			if (coercion_res.hasFailed()) return {};

			base::Optional<Box<code::Expr>> result;
			variant_match(coercion_res.valueOrThrow().getVariant()) {
				variant_case(Coercion, coercion) {
					result = coercion.coerce(ctx.ctx, std::move(element_expr));
				}
				variant_default {
					ctx.ctx.logInt(makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"Cannot coerce collection element type '",
							element_sym_type.toString(),
							"' to iterator type '",
							iter_type.toString(),
							"'."
						),
						ctx.stmt->getIterable().unlock(ctx.ctx)->getStablePosition()
					));
				}
			}
			return result;
		}

		// Build the body of the while loop:
		//   let <iter> = __collection[__idx];
		//   <body>;
		//   __idx = __idx + 1;
		base::Optional<code::CodeBlock> buildWhileBody(
			const ForDesugarCtx& ctx,
			SymID                col_sym,
			SymID                idx_sym,
			SymID                iter_sym,
			tsh::SymbolType<>    iter_type,
			const BodyProcessor& process_body
		) {
			const auto gen = code::generatedOrigin();
			auto idx_ref   = [&] { return makeBox<code::IdentifierExpr>(ctx.ctx, gen, idx_sym); };

			// __collection[__idx]

			Box<code::Expr> collection_expr = [&]() -> Box<code::Expr> {
				if (ctx.collection_is_l_value) {
					// If collection was an l-value we have to dereference it since we
					// operate on it through a ref.
					return makeBox<code::DerefExpr>(
						ctx.ctx, gen, makeBox<code::IdentifierExpr>(ctx.ctx, gen, col_sym)
					);
				} else {
					return makeBox<code::IdentifierExpr>(ctx.ctx, gen, col_sym);
				}
			}();
			Box<code::Expr> raw_element
				= makeBox<code::IndexExpr>(ctx.ctx, gen, std::move(collection_expr), idx_ref());

			auto element_expr = coerceElementToIter(ctx, std::move(raw_element), iter_type);
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
					ctx.u64_mut_type.getType(), 1
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

	SymID getForIteratorSymbol(query::Context& ctx, pst::Access<pst::For> stmt) {
		using GeneratedSymbolData = defgen::GeneratedSymbolData;
		using ControlFlowLocal    = GeneratedSymbolData::ControlFlowLocal;

		auto iter_name = stmt->getIteratorIdentifier().unlock(ctx)->unwrap();
		auto for_scope = ctx.query<QueryPrimaryCodeScopeFor>({ stmt });

		// Get the element type from the iterable.
		auto iterable_pst  = stmt->getIterable().unlock(ctx)->getExpr();
		auto iterable_hout = ctx.query<QueryHoutOfExpr>({ iterable_pst })->valueOrThrow().ref();
		tsh::SymbolType<> iterable_type = iterable_hout->expression_type.getSymbolType();
		auto              iterable_kind = iterable_type.getType().getKind();

		auto element_type = [&]() -> tsh::SymbolType<> {
			switch (iterable_kind) {
			case tsh::Kind::DynamicArray:
				return iterable_type.getType().as<tsh::DynamicArrayAbstractType>().getElementType();
			case tsh::Kind::StaticArray:
				return iterable_type.getType().as<tsh::StaticArrayAbstractType>().getElementType();
			default:
				query::throwFailed();
				CORE_UNREACHABLE();
			}
		}();

		// Get the type if it exists.
		auto              type_holder_opt = stmt->getIteratorType().unlockOpt(ctx);
		tsh::SymbolType<> iter_type       = [&]() -> tsh::SymbolType<> {
            if (type_holder_opt.has_value()) {
                auto maybe_iter_type_pst = type_holder_opt.value()->getExpr().unlockOpt(ctx);
                if (maybe_iter_type_pst.has_value()) {
                    // If a type exists we use it.
                    auto type_ctv = getTypeCTVFromPST(ctx, maybe_iter_type_pst.value());
                    return type_ctv.valueOrThrow().get<tsh::SymbolType<>>().value();
                }
            }
            // Otherwise we infer it from the element type.
            return element_type;
		}();

		if (auto maybe_is_const = stmt->getIsConst(); maybe_is_const.has_value()) {
			iter_type = iter_type.withMutability(
				maybe_is_const.value() ? tsh::Mutability::Immutable : tsh::Mutability::Mutable
			);
		} else {
			// If no let/var exists the element type is the same as the array element type. If the
			// array stores a const than the iterator is const.
		}

		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name                  = iter_name,
			.generated_symbol_data = GeneratedSymbolData{ ControlFlowLocal{
				.owning_scope = for_scope,
				.role         = iter_name,
				.type         = iter_type,
			} },
		});
	}

	base::Optional<code::BlockStmt> desugarFor(
		query::Context& ctx, pst::Access<pst::For> stmt, const BodyProcessor& process_body
	) {
		auto ctx_opt = buildForDesugarCtx(ctx, stmt);
		if (!ctx_opt.has_value()) return {};
		auto& for_ctx = ctx_opt.value();

		// Create the needed symbols.
		SymID col_sym  = makeForLocal(for_ctx, base::StrID("__collection"), for_ctx.col_type);
		SymID idx_sym  = makeForLocal(for_ctx, base::StrID("__index"), for_ctx.u64_mut_type);
		SymID len_sym  = makeForLocal(for_ctx, base::StrID("__len"), for_ctx.u64_immut_type);
		SymID iter_sym = getForIteratorSymbol(ctx, stmt);
		tsh::SymbolType<> iter_type = ctx.query<QueryTypeOfSymbol>(iter_sym)->valueOrThrow();

		// Build the while body first so we bail out on coercion failure before
		// consuming the iterable expression into the __collection variable.
		auto while_body
			= buildWhileBody(for_ctx, col_sym, idx_sym, iter_sym, iter_type, process_body);
		if (!while_body.has_value()) return {};

		// Desugar the loop.
		// while(__idx < __len) {
		// 		let <iter> = __collection[__idx];
		// 		<body>;
		// 		__idx = __idx + 1;
		// }
		code::CodeBlock outer{};
		outer.statements.emplace_back(buildCollectionVar(for_ctx, col_sym));
		outer.statements.emplace_back(buildIndexVar(for_ctx, idx_sym));
		outer.statements.emplace_back(buildLengthVar(for_ctx, len_sym, col_sym));
		outer.statements.emplace_back(makeBox<code::WhileStmt>(
			for_ctx.loop_origin,
			buildCondition(for_ctx, idx_sym, len_sym),
			std::move(while_body.value())
		));

		// Emit it in a block so variable names don't collide if two fors are in the same
		// function and for the collection scope to be not accessible from outside the for loop.
		return code::BlockStmt(for_ctx.loop_origin, std::move(outer));
	}
}

#include "for.hpp"

#include "frontend/pst_parser/access.hpp"
#include "frontend/pst_parser/elements/hierarchy/declarations/for.hpp"
#include "helios/hout/elements/expr.hpp"
#include "helios/hout/elements/stmt.hpp"
#include "helios/hout/origin.hpp"
#include "helios/symbols/query_type_of_symbol.hpp"
#include "helios/symbols/symbol_id.hpp"
#include "helios/tsh/queries/types.hpp"
#include "helios/tsh/symbol_type.hpp"
#include "helios_private/comp_time/comp_time.hpp"
#include "helios_private/hout_creation/expressions/coercions.hpp"
#include "helios_private/hout_creation/expressions/query_hout_of_expr.hpp"
#include "helios_private/scopes/scopes.hpp"
#include "helios_private/symbols/symbols.hpp"

#include "base/collections/optional.hpp"

#include "query_framework/context/context.hpp"
#include "string_id/string_id.hpp"

namespace compiler::helios::desugaring {
	namespace {
		class ForDesugarer final {
		private:
			query::Context&       ctx;
			pst::Access<pst::For> stmt;
			ScopeID               for_scope;

			code::ElementOrigin loop_origin;
			code::ElementOrigin gen_origin;

			tsh::Kind         iterable_kind;
			tsh::SymbolType<> iterable_type;
			Box<code::Expr>   iterable_hout;

			tsh::SymbolType<> u64_mut_type;
			tsh::SymbolType<> u64_immut_type;
			tsh::SymbolType<> col_type;

		public:
			ForDesugarer(
				query::Context&       ctx,
				pst::Access<pst::For> stmt,
				Box<code::Expr>       iterable_hout,
				tsh::SymbolType<>     iterable_type,
				tsh::Kind             iterable_kind
			):
				  ctx(ctx),
				  stmt(stmt),
				  for_scope(ctx.query<QueryPrimaryCodeScopeFor>({ stmt })),
				  loop_origin(code::pstOrigin(stmt)),
				  gen_origin(code::generatedOrigin()),
				  iterable_kind(iterable_kind),
				  iterable_type(iterable_type),
				  iterable_hout(std::move(iterable_hout)),
				  u64_mut_type(tsh::SymbolType<>::withDefaults(
					  tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned)
				  )),
				  u64_immut_type(u64_mut_type.withMutability(tsh::Mutability::Immutable)),
				  col_type(iterable_type.withReferenceKind(tsh::ReferenceKind::Ref)) {}

			base::Optional<code::BlockStmt> desugar(const BodyProcessor& process_body) {
				if (!initializeIterable()) return {};

				if (iterable_kind != tsh::Kind::DynamicArray
				    && iterable_kind != tsh::Kind::StaticArray) {
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						base::strConcat(
							"`for` statements for non-array type: ", iterable_type.toString()
						),
						stmt->getStablePosition()
					));
					return {};
				}
				initializeHelperTypes();

				// Create the needed symbols.
				SymID             col_sym  = makeForLocal(base::StrID("__collection"), col_type);
				SymID             idx_sym  = makeForLocal(base::StrID("__index"), u64_mut_type);
				SymID             len_sym  = makeForLocal(base::StrID("__len"), u64_immut_type);
				SymID             iter_sym = getForIteratorSymbol(ctx, stmt);
				tsh::SymbolType<> iter_type
					= ctx.query<QueryTypeOfSymbol>(iter_sym)->valueOrThrow();

				code::CodeBlock outer{};
				// Create a temp for the collection for it to be evaluated only once before the loop.
				// TODOP: Handle r-values
				// var __collection: ref T = &<iterable>
				outer.statements.emplace_back(buildCollectionVar(col_sym));
				// var __idx: u64 = 0
				outer.statements.emplace_back(buildIndexVar(idx_sym));
				// let __len: u64 = <len __collection> / <constant>
				outer.statements.emplace_back(buildLengthVar(len_sym, col_sym));

				// Desugar the loop.
				// while(__idx < __len) {
				// 		let <iter> = __collection[__idx];
				// 		<body>;
				// 		__idx = __idx + 1;
				// }
				auto condition = buildCondition(idx_sym, len_sym);
				auto while_body
					= buildWhileBody(col_sym, idx_sym, iter_sym, iter_type, process_body);

				if (!while_body.has_value()) return {};

				outer.statements.emplace_back(makeBox<code::WhileStmt>(
					loop_origin, std::move(condition), std::move(while_body.value())
				));

				// Emit it in a block so variable names don't collide if two fors are in the same
				// function and for the collection scope to be not accessible from outside the for loop.
				return code::BlockStmt(loop_origin, std::move(outer));
			}

		private:
			bool initializeIterable() {
				// Get the iterable and it's type.
				auto iterable_pst      = stmt->getIterable().unlock(ctx)->getExpr();
				auto iterable_hout_res = ctx.query<QueryHoutOfExpr>({ iterable_pst });
				if (iterable_hout_res->hasFailed()) return false;

				iterable_hout = iterable_hout_res->valueOrThrow()->clone();
				iterable_type = iterable_hout->expression_type.getSymbolType();
				iterable_kind = iterable_type.getType().getKind();
				return true;
			}

			void initializeHelperTypes() {
				u64_mut_type = tsh::SymbolType<>::withDefaults(
					tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned)
				);
				u64_immut_type = u64_mut_type.withMutability(tsh::Mutability::Immutable);
				col_type       = iterable_type.withReferenceKind(tsh::ReferenceKind::Ref);
			}

			SymID makeForLocal(base::StrID role, tsh::SymbolType<> type) {
				// Compose the name with the current scope hash, so we don't have naming collisions
				// with nested loops.
				auto unique = for_scope.queryUnstablePerfectHash();
				auto name   = base::StrID(base::strConcat(role, unique));

				using GeneratedSymbolData = defgen::GeneratedSymbolData;
				using ControlFlowLocal    = GeneratedSymbolData::ControlFlowLocal;

				return ctx.query<defgen::QueryGeneratedSymbol>({ .name = name,
				                                                 .generated_symbol_data
				                                                 = GeneratedSymbolData{
																	 ControlFlowLocal{
																		 .owning_scope = for_scope,
																		 .role         = role,
																		 .type         = type,
																	 },
																 } });
			}

			Box<code::Stmt> buildCollectionVar(SymID col_sym) {
				// TODOP: What if collection is a r-value;
				return makeBox<code::VariableStmt>(
					loop_origin,
					makeBox<code::RefOfExpr>(ctx, gen_origin, std::move(iterable_hout)),
					col_type,
					col_sym
				);
			}

			Box<code::Stmt> buildIndexVar(SymID idx_sym) {
				return makeBox<code::VariableStmt>(
					loop_origin,
					makeBox<code::DefaultValueExpr>(ctx, gen_origin, u64_mut_type.getType()),
					u64_mut_type,
					idx_sym
				);
			}

			Box<code::Stmt> buildLengthVar(SymID len_sym, SymID col_sym) {
				// Length is calculated once before the loop.
				auto len_expr = [&]() -> Box<code::Expr> {
					switch (iterable_kind) {
					case tsh::Kind::DynamicArray: {
						return makeBox<code::UnaryOperatorExpr>(
							ctx,
							gen_origin,
							code::BuiltinUnary::Len,
							makeBox<code::DerefExpr>(
								ctx,
								gen_origin,
								makeBox<code::IdentifierExpr>(ctx, gen_origin, col_sym)
							)
						);
					}
					case tsh::Kind::StaticArray: {
						auto size
							= iterable_type.getType().as<tsh::StaticArrayAbstractType>().getSize();
						return makeBox<code::LiteralNumericExpr>(
							ctx,
							gen_origin,
							compiler::numeric_value::NumericValue::createOfType<u64>(
								u64_immut_type.getType(), size
							)
								.value()
						);
					}
					default:
						CORE_UNREACHABLE();
					}
				}();
				return makeBox<code::VariableStmt>(
					loop_origin, std::move(len_expr), u64_immut_type, len_sym
				);
			}

			Box<code::Expr> buildCondition(SymID idx_sym, SymID len_sym) {
				return makeBox<code::BinaryOperatorExpr>(
					ctx,
					gen_origin,
					code::BuiltinBinary::IntegerLt,
					makeBox<code::IdentifierExpr>(ctx, gen_origin, idx_sym),
					makeBox<code::IdentifierExpr>(ctx, gen_origin, len_sym)
				);
			}

			base::Optional<code::CodeBlock> buildWhileBody(
				SymID                col_sym,
				SymID                idx_sym,
				SymID                iter_sym,
				tsh::SymbolType<>    iter_type,
				const BodyProcessor& process_body
			) {
				code::CodeBlock while_body{};

				auto col_ref
					= [&] { return makeBox<code::IdentifierExpr>(ctx, gen_origin, col_sym); };
				auto idx_ref
					= [&] { return makeBox<code::IdentifierExpr>(ctx, gen_origin, idx_sym); };
				auto derefed_col_ref
					= [&] { return makeBox<code::DerefExpr>(ctx, gen_origin, col_ref()); };

				Box<code::Expr> element_expr
					= makeBox<code::IndexExpr>(ctx, gen_origin, derefed_col_ref(), idx_ref());
				tsh::SymbolType<> element_sym_type = element_expr->expression_type.getSymbolType();

				auto coercion_res = canCoerce(ctx, element_sym_type, iter_type);
				if (coercion_res.hasFailed()) return {};

				bool coercion_success = true;
				variant_match(coercion_res.valueOrThrow().getVariant()) {
					variant_case(Coercion, coercion) {
						element_expr = coercion.coerce(ctx, std::move(element_expr));
					}
					variant_default {
						ctx.logInt(makeBox<dia_int::PlaceholderError>(
							base::strConcat(
								"Cannot coerce collection element type '",
								element_sym_type.toString(),
								"' to iterator type '",
								iter_type.toString(),
								"'."
							),
							stmt->getIterable().unlock(ctx)->getStablePosition()
						));
						coercion_success = false;
					}
				}

				if (!coercion_success) return {};

				// let <user_var> = __collection[__idx];
				while_body.statements.emplace_back(makeBox<code::VariableStmt>(
					loop_origin, std::move(element_expr), iter_type, iter_sym
				));

				// <body>;
				auto body = process_body(stmt->getBody());
				for (auto& s: body.statements) while_body.statements.emplace_back(std::move(s));

				// __idx = __idx + 1;
				auto one = makeBox<code::LiteralNumericExpr>(
					ctx,
					gen_origin,
					compiler::numeric_value::NumericValue::createOfType<u64>(
						u64_mut_type.getType(), 1
					)
						.value()
				);

				while_body.statements.emplace_back(makeBox<code::AssignmentStmt>(
					gen_origin,
					idx_ref(),
					makeBox<code::BinaryOperatorExpr>(
						ctx, gen_origin, code::BuiltinBinary::IntegerAdd, idx_ref(), std::move(one)
					)
				));

				return while_body;
			}
		};
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
			// If no let/var exists the element type is the same as the array element type. Is array
			// stores a const than the iterator is const.
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
		auto iterable_pst      = stmt->getIterable().unlock(ctx)->getExpr();
		auto iterable_hout_res = ctx.query<QueryHoutOfExpr>({ iterable_pst });
		if (iterable_hout_res->hasFailed()) return {};

		auto iterable_hout = iterable_hout_res->valueOrThrow()->clone();
		auto iterable_type = iterable_hout->expression_type.getSymbolType();
		auto iterable_kind = iterable_type.getType().getKind();

		return ForDesugarer{ ctx, stmt, std::move(iterable_hout), iterable_type, iterable_kind }
		    .desugar(process_body);
	}
}

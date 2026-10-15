// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "for.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/lang_primitives.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/kind.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios/tsh/value_category.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/definition_generation/length_methods.hpp>
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>
#include <helios_private/hout_creation/expressions/coercions/errors.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/lookup/interface.hpp>
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
			ForIterableInfo   iterable_info;
			bool              collection_is_l_value;
			Box<code::Expr>   iterable_hout;
		};

		tsh::SymbolType<> getI64(query::Context& ctx) {
			return tsh::SymbolType<>::withDefaults(
				tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed)
			);
		}

		tsh::SymbolType<> getConstI64(query::Context& ctx) {
			return tsh::SymbolType<>::withDefaults(
					   tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed)
			)
			    .withMutability(tsh::Mutability::Immutable);
		}

		/**
		 * @brief Looks up the single method named @p method_name on @p type. Returns an empty
		 * optional when there is no such method or when it is overloaded.
		 */
		base::Optional<SymID> lookupSingleMethod(
			query::Context& ctx, tsh::AbstractType type, std::string_view method_name
		) {
			const auto& leaves = HInterface::ofTypeInstance(type)
			                         .lookup(ctx, base::StrID(method_name))
			                         ->valueOrThrow()
			                         .leaves;
			if (leaves.size() != 1) return {};
			return leaves.front();
		}

		bool isTemporaryRangeType(query::Context& ctx, tsh::AbstractType type) {
			if (not isLanguagePrimitivePresent(ctx, LanguagePrimitive::TemporaryRange))
				return false;
			const SymID range_sym
				= ctx.query<QueryLanguagePrimitiveSymID>({ LanguagePrimitive::TemporaryRange })
			          ->valueOrThrow();
			return type == tsh::AbstractType(ctx.query<tsh::QueryClassType>(range_sym));
		}

		/**
		 * @brief Returns `T` when @p type is `List[T]`. The element type is read from the
		 * return type of `at()` (`ref T`), then confirmed by baking `List[T]` again.
		 */
		base::Optional<tsh::SymbolType<>> listElementType(
			query::Context& ctx, tsh::AbstractType type
		) {
			if (not isLanguagePrimitivePresent(ctx, LanguagePrimitive::List)) return {};
			// Cheap filter, so other classes with an `at()` method don't bake a `List`.
			if (name(type.as<tsh::ClassAbstractType>().getSymbol()) != base::StrID("List"))
				return {};

			auto at_sym = lookupSingleMethod(ctx, type, "at");
			if_opt_none(at_sym) return {};
			auto at_type = ctx.query<QueryTypeOfSymbol>(at_sym.value())->valueOrThrow().getType();
			if (at_type.getKind() != tsh::Kind::Function) return {};

			auto result_type = at_type.as<tsh::FunctionAbstractType>().getResultType();
			if (result_type.getRefKind() != tsh::ReferenceKind::Ref) return {};
			auto element_type = result_type.withReferenceKind(tsh::ReferenceKind::Direct);

			const SymID baked_list
				= bakeLanguagePrimitiveWithTypes(ctx, LanguagePrimitive::List, { element_type });
			if (type != tsh::AbstractType(ctx.query<tsh::QueryClassType>(baked_list))) return {};
			return element_type;
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
			bool              iterable_is_r_value
				= not iterable_hout->expression_type.getValueCategory().canBeAssignedTo();

			auto iterable_info = getForIterableInfo(ctx, iterable_type);
			if_opt_none(iterable_info) {
				ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
					base::strConcat(
						"`for` statements over type: ",
						iterable_type.toString(),
						". Only static arrays, ranges (`n..m`) and `List` are supported."
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
				.iterable_info         = iterable_info.value(),
				.collection_is_l_value = not iterable_is_r_value,
				.iterable_hout         = std::move(iterable_hout),
			};
		}

		// If the iterable is an l-value the reusable holds a reference to it, so references are
		// dereferenced before the collection is accessed.
		Box<code::Expr> derefCollection(const ForDesugarCtx& ctx, Box<code::Expr> collection) {
			bool needs_deref = ctx.iterable_type.getRefKind() != tsh::ReferenceKind::Direct
			                || ctx.collection_is_l_value;
			if (needs_deref)
				return makeBox<code::DerefExpr>(
					ctx.ctx, code::generatedOrigin(), std::move(collection)
				);
			return collection;
		}

		// var __idx: i64 = <start>
		Box<code::Stmt> buildIndexVar(
			const ForDesugarCtx& ctx, SymID idx_sym, Box<code::Expr> start
		) {
			return makeBox<code::VariableStmt>(
				code::generatedOrigin(), std::move(start), getI64(ctx.ctx), idx_sym
			);
		}

		// Length is calculated once before the loop.
		// let __len: const i64 = <length>
		Box<code::Stmt> buildLengthVar(
			const ForDesugarCtx& ctx, SymID len_sym, Box<code::Expr> length
		) {
			return makeBox<code::VariableStmt>(
				code::generatedOrigin(), std::move(length), getConstI64(ctx.ctx), len_sym
			);
		}

		// <len __collection> for static arrays.
		Box<code::Expr> buildStaticArrayLength(
			const ForDesugarCtx& ctx, Box<code::Expr> iterable_reusable_opt
		) {
			const auto gen = code::generatedOrigin();

			SymID length_method_sym
				= defgen::lengthMethodForType(ctx.ctx, ctx.iterable_type.getType());

			Box<code::Expr> length_arg = std::move(iterable_reusable_opt);
			if (not ctx.collection_is_l_value)
				length_arg = makeBox<code::RefOfExpr>(ctx.ctx, gen, std::move(length_arg));

			std::vector<Box<code::Expr>> length_args;
			length_args.emplace_back(std::move(length_arg));

			return makeBox<code::CallExpr>(
				ctx.ctx,
				gen,
				makeBox<code::IdentifierExpr>(ctx.ctx, gen, length_method_sym),
				std::move(length_args)
			);
		}

		// The method of the `List` (or field of the `TemporaryRange`) the desugaring relies on.
		// Logs an error when the standard library doesn't provide it.
		base::Optional<SymID> requireMember(
			const ForDesugarCtx& ctx, std::string_view member_name, bool is_method
		) {
			const auto type = ctx.iterable_type.getType();

			base::Optional<SymID> member;
			if (is_method) {
				member = lookupSingleMethod(ctx.ctx, type, member_name);
			} else {
				const auto& fields
					= type.getInterface(ctx.ctx)->getElementsWithName(base::StrID(member_name));
				if (fields.size() == 1) member = fields.front().getSymbol();
			}

			if_opt_none(member) {
				ctx.ctx.logInt(makeBox<dia::PlaceholderError>(
					base::strConcat(
						"`for` over '",
						ctx.iterable_type.toString(),
						"' needs exactly one `",
						member_name,
						is_method ? "` method." : "` field."
					),
					ctx.iterable_origin.getSourcePosition(ctx.ctx)
				));
			}
			return member;
		}

		/**
		 * @brief The three per-kind pieces of a desugared `for`: the first index, the length and
		 * the element at `__index` (before it is coerced to the iterator type).
		 */
		struct LoopParts final {
			Box<code::Expr> index_start;
			Box<code::Expr> length;
			Box<code::Expr> raw_element;
		};

		// The `ReusableExpr` owner has to end up in the statement that runs first, every later
		// read of the iterable uses `collection_next_use`.
		base::Optional<LoopParts> buildLoopParts(
			const ForDesugarCtx& ctx,
			Box<code::Expr>      iterable_reusable,
			Box<code::Expr>      collection_next_use,
			SymID                idx_sym
		) {
			code::shorthands::Shorthand s{ ctx.ctx };
			auto zero = [&] { return s.defaultValue(getI64(ctx.ctx).getType()); };

			switch (ctx.iterable_info.kind) {
			case ForIterableKind::StaticArray:
				// __index = 0; __len = len __collection; element = __collection[__index]
				return LoopParts{
					.index_start = zero(),
					.length      = buildStaticArrayLength(ctx, std::move(iterable_reusable)),
					.raw_element = s.index(
						derefCollection(ctx, std::move(collection_next_use)), s.ident(idx_sym)
					),
				};
			case ForIterableKind::List: {
				// __index = 0; __len = __collection.length();
				// element = copy __collection.at(__index)
				auto length_sym = requireMember(ctx, "length", true);
				auto at_sym     = requireMember(ctx, "at", true);
				if (not length_sym.has_value() or not at_sym.has_value()) return {};

				// `at()` returns `ref T`, and a non-trivial `T` can't be copied out of a
				// reference implicitly, so the copy is spelled out here.
				const auto& element_type = ctx.iterable_info.element_type;
				if (not element_type.isCopyable(ctx.ctx)) {
					ctx.ctx.logInt(makeBox<dia::PlaceholderError>(
						base::strConcat(
							"`for` over a `List` copies every element, but `",
							element_type.toString(),
							"` cannot be copied."
						),
						ctx.iterable_origin.getSourcePosition(ctx.ctx)
					));
					return {};
				}
				auto element_ref = s.call(
					s.ident(at_sym.value()),
					s.prepToPassSelf(std::move(collection_next_use)),
					s.ident(idx_sym)
				);
				return LoopParts{
					.index_start = zero(),
					.length      = s.call(
                        s.ident(length_sym.value()), s.prepToPassSelf(std::move(iterable_reusable))
                    ),
					.raw_element = s.copyValue(s.deref(std::move(element_ref))),
				};
			}
			case ForIterableKind::Range: {
				// __index = __range.start; __len = __range.end; element = __index
				auto start_sym = requireMember(ctx, "start", false);
				auto end_sym   = requireMember(ctx, "end", false);
				if (not start_sym.has_value() or not end_sym.has_value()) return {};
				return LoopParts{
					.index_start = s.access(
						derefCollection(ctx, std::move(iterable_reusable)), start_sym.value()
					),
					.length = s.access(
						derefCollection(ctx, std::move(collection_next_use)), end_sym.value()
					),
					.raw_element = s.ident(idx_sym),
				};
			}
			}
			CORE_UNREACHABLE();
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

		// let <iter> = <raw_element>;
		// <body>;
		base::Optional<code::CodeBlock> buildWhileBody(
			const ForDesugarCtx& ctx,
			Box<code::Expr>      raw_element,
			SymID                iter_sym,
			tsh::SymbolType<>    iter_type,
			const BodyProcessor& process_body
		) {
			// The iterator is a copy of the element, so changing it doesn't change the iterable.
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
			if (element_expr.hasFailed()) return {};

			code::CodeBlock body{};

			// let <user_var> = <element>;
			body.statements.emplace_back(makeBox<code::VariableStmt>(
				ctx.iterator_origin, std::move(element_expr.valueOrPanic()), iter_type, iter_sym
			));

			// <body>;
			auto user_body = process_body(ctx.stmt->getBody());
			for (auto& s: user_body.statements) body.statements.emplace_back(std::move(s));

			return body;
		}

		// __idx = __idx + 1; this runs after the body and on continue.
		Box<code::Stmt> buildForStep(const ForDesugarCtx& ctx, SymID idx_sym) {
			const auto gen = code::generatedOrigin();
			auto idx_ref   = [&] { return makeBox<code::IdentifierExpr>(ctx.ctx, gen, idx_sym); };

			auto one = makeBox<code::LiteralNumericExpr>(
				ctx.ctx,
				gen,
				compiler::numeric_value::NumericValue::createOfType<i64>(
					getI64(ctx.ctx).getType(), 1
				)
					.value()
			);
			return makeBox<code::AssignmentStmt>(
				gen,
				idx_ref(),
				makeBox<code::BinaryOperatorExpr>(
					ctx.ctx, gen, code::BuiltinBinary::IntegerAdd, idx_ref(), std::move(one)
				)
			);
		}
	}

	base::Optional<ForIterableInfo> getForIterableInfo(
		query::Context& ctx, const tsh::SymbolType<>& iterable_type
	) {
		const auto type = iterable_type.getType();
		switch (type.getKind()) {
		case tsh::Kind::StaticArray:
			return ForIterableInfo{
				.kind         = ForIterableKind::StaticArray,
				.element_type = type.as<tsh::StaticArrayAbstractType>().getElementType(),
			};
		case tsh::Kind::Class:
			if (isTemporaryRangeType(ctx, type))
				return ForIterableInfo{ .kind         = ForIterableKind::Range,
					                    .element_type = getI64(ctx) };
			if (auto element_type = listElementType(ctx, type))
				return ForIterableInfo{
					.kind         = ForIterableKind::List,
					.element_type = element_type.value(),
				};
			return {};
		default:
			return {};
		}
	}

	ForGeneratedSymbols getForGeneratedSymbols(query::Context& ctx, pst::Access<pst::For> stmt) {
		ScopeID for_scope = ctx.query<QueryPrimaryCodeScopeFor>({ stmt });

		auto get_generated_local = [&](base::StrID role, tsh::SymbolType<> type) {
			// No discriminator in the name: it is part of the query key, so anything derived from
			// the loop (scope id, position, hash) would differ per compilation or move on edits.
			auto name = role;

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

		return {
			.iterator
			= ctx.query<QuerySymbolOfSTMT>({ stmt->getIteratorIdentifier() }).valueOrThrow(),
			.index  = get_generated_local(base::StrID("__index"), getI64(ctx)),
			.length = get_generated_local(base::StrID("__len"), getConstI64(ctx)),
		};
	}

	base::Optional<code::BlockStmt> desugarFor(
		query::Context&       ctx,
		pst::Access<pst::For> stmt,
		base::Optional<SymID> control_flow_id,
		const BodyProcessor&  process_body
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

		auto loop_parts = buildLoopParts(
			for_ctx, std::move(iterable_reusable), std::move(iterable_next_use), symbols.index
		);
		if (!loop_parts.has_value()) return {};

		auto while_body = buildWhileBody(
			for_ctx, std::move(loop_parts->raw_element), symbols.iterator, iter_type, process_body
		);
		if (!while_body.has_value()) return {};

		// Desugar the loop.
		// var __index : i64 = <index_start>;
		// var __len: const i64 = <length>;
		// while(__idx < __len) {
		// 		let <iter> = <raw_element>;
		// 		<body>;
		// 		__idx = __idx + 1;
		// }
		code::CodeBlock outer{};
		outer.statements.emplace_back(
			buildIndexVar(for_ctx, symbols.index, std::move(loop_parts->index_start))
		);
		outer.statements.emplace_back(
			buildLengthVar(for_ctx, symbols.length, std::move(loop_parts->length))
		);

		outer.statements.emplace_back(makeBox<code::WhileStmt>(
			for_ctx.loop_origin,
			buildCondition(for_ctx, symbols.index, symbols.length),
			std::move(while_body.value()),
			control_flow_id,
			code::ControlFlowKind::For,
			buildForStep(for_ctx, symbols.index)
		));

		// Emit it in a block so variable names don't collide if two fors are in the same
		// function and for the collection scope to be not accessible from outside the for loop.
		return code::BlockStmt(for_ctx.loop_origin, std::move(outer));
	}
}

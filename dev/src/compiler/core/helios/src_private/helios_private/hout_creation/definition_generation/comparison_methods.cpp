// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "comparison_methods.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <algorithm>
#include <ranges>

namespace compiler::helios::defgen {
	using namespace code::shorthands;

	const ComparisonMethodInfo& comparisonMethodInfo(const Method::Kind kind) {
		const auto* const info
			= std::ranges::find(COMPARISON_METHODS, kind, &ComparisonMethodInfo::kind);
		CORE_ASSERT(info != COMPARISON_METHODS.end(), "Expected a comparison method kind.");
		return *info;
	}

	SymID comparisonSymForType(
		query::Context& ctx, const tsh::AbstractType type, const Method::Kind kind
	) {
		const auto comparison
			= type.getInterface(ctx)->getSpecialElement(comparisonMethodInfo(kind).special_kind);
		CORE_ASSERT(
			comparison.has_value(), "Every type carrying information should have comparisons."
		);
		return comparison.value()->getSymbol();
	}

	SymID generatedComparisonSymForType(
		query::Context& ctx, const tsh::AbstractType type, const Method::Kind kind
	) {
		return ctx.query<QueryGeneratedSymbol>({
			.name                  = base::StrID(comparisonMethodInfo(kind).spelling),
			.generated_symbol_data = Method{ .owner_type = type, .kind = kind },
		});
	}

	namespace {
		/**
		 * @brief The value of the operand at @p idx of a comparison operator. Operands of simple
		 * types are passed by value, the others by reference.
		 */
		Box<code::Expr> operandValue(
			const Shorthand& s, const HOUTFunctionDeclaration& decl, const usize idx
		) {
			const auto& param = decl.parameters.at(idx);
			auto        ident = s.ident(param.helios_symbol);
			if (param.type.getRefKind() == tsh::ReferenceKind::Direct) return ident;
			return s.deref(std::move(ident));
		}

		/**
		 * @brief Calls the comparison operator @p kind of @p type, which may be user-defined.
		 */
		Box<code::Expr> callComparison(
			query::Context&         ctx,
			const tsh::AbstractType type,
			const Method::Kind      kind,
			Box<code::Expr>         lhs,
			Box<code::Expr>         rhs
		) {
			const Shorthand s{ ctx };
			const SymID     comparison_sym = comparisonSymForType(ctx, type, kind);
			// The right-hand side of a user-defined operator may be taken by value.
			const auto rhs_type = ctx.query<QueryTypeOfSymbol>(comparison_sym)
			                          ->valueOrThrow()
			                          .getType()
			                          .as<tsh::FunctionAbstractType>()
			                          .getParameterTypes()
			                          .at(1);
			return s.call(
				s.ident(comparison_sym),
				s.prepToPassSelf(std::move(lhs)),
				s.coerce(s.prepToPassSelf(std::move(rhs)), rhs_type)
			);
		}

		/**
		 * @brief The fields taking part in the comparison of an aggregate. Fields which don't
		 * carry information are always equal, so they are skipped.
		 */
		std::vector<tsh::InterfaceElement> comparedFields(
			query::Context& ctx, const tsh::AbstractType aggregate_type
		) {
			return aggregate_type.getInterface(ctx)->getFieldsView()
			     | std::views::filter([&](const tsh::InterfaceElement& field) {
					   return field.getType(ctx).getType().carriesInformation(ctx);
				   })
			     | std::ranges::to<std::vector>();
		}

		/**
		 * @brief Calls the comparison operator @p kind on the field @p field of the operands
		 * at @p lhs_idx and @p rhs_idx.
		 */
		Box<code::Expr> compareFields(
			query::Context&                ctx,
			const HOUTFunctionDeclaration& decl,
			const tsh::InterfaceElement&   field,
			const Method::Kind             kind,
			const usize                    lhs_idx,
			const usize                    rhs_idx
		) {
			const Shorthand s{ ctx };
			return callComparison(
				ctx,
				field.getType(ctx).getType(),
				kind,
				s.access(operandValue(s, decl, lhs_idx), field.getSymbol()),
				s.access(operandValue(s, decl, rhs_idx), field.getSymbol())
			);
		}

		/**
		 * @brief `return self <op> other;`, for types compared with a builtin operation.
		 */
		void compareBuiltin(
			query::Context&                ctx,
			const HOUTFunctionDeclaration& decl,
			std::vector<Box<code::Stmt>>&  body,
			const code::BuiltinBinary      operation
		) {
			const Shorthand s{ ctx };
			body.emplace_back(
				s.ret(s.binOp(operandValue(s, decl, 0), operation, operandValue(s, decl, 1)))
			);
		}

		[[noreturn]] void comparisonNotYetImplemented(
			query::Context& ctx, const Method::Kind kind, const tsh::AbstractType type
		) {
			ctx.log<dia::NotYetImplementedCodeError>(
				base::strConcat(
					"The `",
					comparisonMethodInfo(kind).spelling,
					"` operator is not yet implemented for the type `",
					type.toString(),
					"`."
				),
				"Comparison operators are generated for numbers, characters, booleans, pointers, "
				"tuples and classes. Pointers and values of type `type` can only be compared for "
				"equality."
			);
			query::throwFailed();
			CORE_UNREACHABLE();
		}

		HOUTFunction makeComparisonFunction(
			const HOUTFunctionDeclaration& decl, std::vector<Box<code::Stmt>> body
		) {
			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}
	}

	struct IMPLEMENT_QUERY(QueryEqualMethod, query::QResult<HOUTFunction>) {
		/**
		 * @brief The fields are compared in order, and the first unequal one decides.
		 */
		static void compareAggregate(
			Context&                       ctx,
			const HOUTFunctionDeclaration& decl,
			std::vector<Box<code::Stmt>>&  body,
			const tsh::AbstractType        aggregate_type
		) {
			const Shorthand s{ ctx };
			for (const auto& field: comparedFields(ctx, aggregate_type)) {
				auto fields_equal = compareFields(ctx, decl, field, Method::Kind::Equal, 0, 1);
				body.emplace_back(s.ifStmt(
					s.unOp(code::BuiltinUnary::BooleanNot, std::move(fields_equal)),
					{ s.ret(s.litBool(false)) }
				));
			}
			body.emplace_back(s.ret(s.litBool(true)));
		}

		static PResult provide(Context& ctx, const QKey owner_type) {
			const auto equal_sym
				= generatedComparisonSymForType(ctx, owner_type, Method::Kind::Equal);
			const auto& decl = ctx.query<QueryDeclOfFun>(equal_sym)->valueOrThrow();

			std::vector<Box<code::Stmt>> body{};

			switch (owner_type.getKind()) {
			case tsh::Kind::Byte:
			case tsh::Kind::Bool:
			case tsh::Kind::Char:
			case tsh::Kind::Integral:
			case tsh::Kind::RawPointer:
			case tsh::Kind::Pointer:
			case tsh::Kind::ManyPointer:
			case tsh::Kind::CPointer: {
				compareBuiltin(ctx, decl, body, code::BuiltinBinary::IntegerEq);
				break;
			}
			case tsh::Kind::Float: {
				compareBuiltin(ctx, decl, body, code::BuiltinBinary::FloatEq);
				break;
			}
			case tsh::Kind::Meta: {
				compareBuiltin(ctx, decl, body, code::BuiltinBinary::MetaEq);
				break;
			}
			case tsh::Kind::Tuple:
			case tsh::Kind::Class: {
				compareAggregate(ctx, decl, body, owner_type);
				break;
			}
			default: {
				comparisonNotYetImplemented(ctx, Method::Kind::Equal, owner_type);
			}
			}

			return makeComparisonFunction(decl, std::move(body));
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryEqualMethod);

	struct IMPLEMENT_QUERY(QueryLessMethod, query::QResult<HOUTFunction>) {
		/**
		 * @brief Lexicographic order: the first field in which the operands differ decides.
		 * Only `<` is used on the fields.
		 */
		static void compareAggregate(
			Context&                       ctx,
			const HOUTFunctionDeclaration& decl,
			std::vector<Box<code::Stmt>>&  body,
			const tsh::AbstractType        aggregate_type
		) {
			const Shorthand s{ ctx };
			for (const auto& field: comparedFields(ctx, aggregate_type)) {
				body.emplace_back(s.ifStmt(
					compareFields(ctx, decl, field, Method::Kind::Lesser, 0, 1),
					{ s.ret(s.litBool(true)) }
				));
				body.emplace_back(s.ifStmt(
					compareFields(ctx, decl, field, Method::Kind::Lesser, 1, 0),
					{ s.ret(s.litBool(false)) }
				));
			}
			body.emplace_back(s.ret(s.litBool(false)));
		}

		static PResult provide(Context& ctx, const QKey owner_type) {
			const auto less_sym
				= generatedComparisonSymForType(ctx, owner_type, Method::Kind::Lesser);
			const auto& decl = ctx.query<QueryDeclOfFun>(less_sym)->valueOrThrow();

			std::vector<Box<code::Stmt>> body{};

			switch (owner_type.getKind()) {
			case tsh::Kind::Byte:
			case tsh::Kind::Bool:
			case tsh::Kind::Char:
			case tsh::Kind::Integral: {
				compareBuiltin(ctx, decl, body, code::BuiltinBinary::IntegerLt);
				break;
			}
			case tsh::Kind::Float: {
				compareBuiltin(ctx, decl, body, code::BuiltinBinary::FloatLt);
				break;
			}
			case tsh::Kind::Tuple:
			case tsh::Kind::Class: {
				compareAggregate(ctx, decl, body, owner_type);
				break;
			}
			default: {
				comparisonNotYetImplemented(ctx, Method::Kind::Lesser, owner_type);
			}
			}

			return makeComparisonFunction(decl, std::move(body));
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLessMethod);

	struct IMPLEMENT_QUERY(QueryDerivedComparisonMethod, query::QResult<HOUTFunction>) {
		/**
		 * @brief How a comparison is computed from `==` or `<`, e.g. `a >= b` is `not (a < b)`.
		 */
		struct Derivation final {
			Method::Kind base_kind;
			bool         swap_operands;
			bool         negate;
		};

		static PResult provide(Context& ctx, const QKey comparison_sym) {
			static const base::Map<Method::Kind, Derivation> derivations = {
				{ Method::Kind::NotEqual,
				  { .base_kind = Method::Kind::Equal, .swap_operands = false, .negate = true } },
				{ Method::Kind::Greater,
				  { .base_kind = Method::Kind::Lesser, .swap_operands = true, .negate = false } },
				{ Method::Kind::LessEqual,
				  { .base_kind = Method::Kind::Lesser, .swap_operands = true, .negate = true } },
				{ Method::Kind::GreaterEqual,
				  { .base_kind = Method::Kind::Lesser, .swap_operands = false, .negate = true } },
			};

			const auto& method     = std::get<Method>(getSymRef(comparison_sym)->other);
			const auto& derivation = derivations.at(method.kind);
			const auto& decl       = ctx.query<QueryDeclOfFun>(comparison_sym)->valueOrThrow();

			const Shorthand s{ ctx };
			const usize     lhs_idx = derivation.swap_operands ? 1 : 0;
			const usize     rhs_idx = derivation.swap_operands ? 0 : 1;

			Box<code::Expr> result = callComparison(
				ctx,
				method.owner_type,
				derivation.base_kind,
				operandValue(s, decl, lhs_idx),
				operandValue(s, decl, rhs_idx)
			);
			if (derivation.negate)
				result = s.unOp(code::BuiltinUnary::BooleanNot, std::move(result));

			std::vector<Box<code::Stmt>> body{};
			body.emplace_back(s.ret(std::move(result)));
			return makeComparisonFunction(decl, std::move(body));
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDerivedComparisonMethod);
}

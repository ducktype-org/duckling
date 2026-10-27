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
#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/hout_creation/expressions/operators.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	using namespace code::shorthands;

	namespace {
		lexer::Operator equalOp() {
			static const lexer::Operator equal_op{ base::StrID("==") };
			return equal_op;
		}

		lexer::Operator lessOp() {
			static const lexer::Operator less_op{ base::StrID("<") };
			return less_op;
		}

		bool isEqualityKind(const Method::Kind kind) {
			return kind == Method::Kind::Equal or kind == Method::Kind::NotEqual;
		}

		/**
		 * @brief Whether a field of type @p field_type supports @p base_kind, which is either
		 * `Equal` or `Less` — the only two operators a generated body applies to fields.
		 */
		bool fieldSupports(
			query::Context& ctx, const tsh::AbstractType field_type, const Method::Kind base_kind
		) {
			switch (field_type.getKind()) {
			case tsh::Kind::Integral:
			case tsh::Kind::Float:
			case tsh::Kind::Char:
				return true;
			case tsh::Kind::Bool:
			case tsh::Kind::Meta:
				return base_kind == Method::Kind::Equal;
			case tsh::Kind::Tuple:
			case tsh::Kind::Class:
				return canGenerateComparison(ctx, field_type, base_kind);
			default:
				return false;
			}
		}
	}

	base::Optional<Method::Kind> comparisonKindForOperator(const lexer::Operator op) {
		const auto spelling = op.value.strView();
		if (spelling == "==") return Method::Kind::Equal;
		if (spelling == "!=") return Method::Kind::NotEqual;
		if (spelling == "<") return Method::Kind::Less;
		if (spelling == "<=") return Method::Kind::LessEqual;
		if (spelling == ">") return Method::Kind::Greater;
		if (spelling == ">=") return Method::Kind::GreaterEqual;
		return {};
	}

	SymID generatedComparisonSymForType(
		query::Context& ctx, const tsh::AbstractType type, const Method::Kind kind
	) {
		CORE_ASSERT(isComparisonMethodKind(kind), "Expected a comparison method kind.");
		const auto spelling = [&] {
			switch (kind) {
			case Method::Kind::Equal:
				return "==";
			case Method::Kind::NotEqual:
				return "!=";
			case Method::Kind::Less:
				return "<";
			case Method::Kind::LessEqual:
				return "<=";
			case Method::Kind::Greater:
				return ">";
			case Method::Kind::GreaterEqual:
				return ">=";
			default:
				CORE_UNREACHABLE();
			}
		}();
		return ctx.query<QueryGeneratedSymbol>({
			.name                  = base::StrID(spelling),
			.generated_symbol_data = Method{ .owner_type = type, .kind = kind },
		});
	}

	bool canGenerateComparison(
		query::Context& ctx, const tsh::AbstractType type, const Method::Kind kind
	) {
		if (type.getKind() != tsh::Kind::Tuple and type.getKind() != tsh::Kind::Class) return false;
		// @TODO: #1956 Methods don't work for zero-sized types yet, due to taking ref to self
		if (not type.carriesInformation(ctx)) return false;

		const auto base_kind = isEqualityKind(kind) ? Method::Kind::Equal : Method::Kind::Less;
		// A derived operator is built from its base operator only.
		if (kind != base_kind) return canGenerateComparison(ctx, type, base_kind);

		return std::ranges::all_of(
			type.getInterface(ctx)->getFieldsView(),
			[&](const tsh::InterfaceElement& field) {
				return fieldSupports(ctx, field.getType(ctx).getType(), base_kind);
			}
		);
	}

	struct IMPLEMENT_QUERY(QueryComparisonMethod, query::QResult<HOUTFunction>) {
		/**
		 * @brief `a == b` for every field, joined with `and`.
		 */
		static Box<code::Expr> generateEqual(
			Context&                            ctx,
			const std::vector<Box<code::Expr>>& lhs_fields,
			const std::vector<Box<code::Expr>>& rhs_fields
		) {
			const Shorthand s{ ctx };
			Box<code::Expr> result = code::resolveBinaryOperatorInGeneratedCode(
				ctx, equalOp(), lhs_fields.back()->clone(), rhs_fields.back()->clone()
			);
			for (usize idx = lhs_fields.size() - 1; idx-- > 0;) {
				auto fields_equal = code::resolveBinaryOperatorInGeneratedCode(
					ctx, equalOp(), lhs_fields[idx]->clone(), rhs_fields[idx]->clone()
				);
				result = s.binOp(
					std::move(fields_equal), code::BuiltinBinary::BooleanAnd, std::move(result)
				);
			}
			return result;
		}

		/**
		 * @brief Lexicographic `<`: the first field where one side is less decides. Uses only `<`
		 * on the fields, so `a < b or (not (b < a) and <the rest>)`.
		 */
		static Box<code::Expr> generateLess(
			Context&                            ctx,
			const std::vector<Box<code::Expr>>& lhs_fields,
			const std::vector<Box<code::Expr>>& rhs_fields
		) {
			const Shorthand s{ ctx };
			auto            less = [&](const usize idx, const bool swapped) {
                const auto& lhs = swapped ? rhs_fields[idx] : lhs_fields[idx];
                const auto& rhs = swapped ? lhs_fields[idx] : rhs_fields[idx];
                return code::resolveBinaryOperatorInGeneratedCode(
                    ctx, lessOp(), lhs->clone(), rhs->clone()
                );
			};

			Box<code::Expr> result = less(lhs_fields.size() - 1, false);
			for (usize idx = lhs_fields.size() - 1; idx-- > 0;) {
				auto not_greater = s.unOp(code::BuiltinUnary::BooleanNot, less(idx, true));
				auto rest        = s.binOp(
                    std::move(not_greater), code::BuiltinBinary::BooleanAnd, std::move(result)
                );
				result = s.binOp(less(idx, false), code::BuiltinBinary::BooleanOr, std::move(rest));
			}
			return result;
		}

		/**
		 * @brief The boolean expression computing the comparison @p method of `self` and `other`.
		 */
		static Box<code::Expr> generateResult(
			Context& ctx, const Method& method, const HOUTFunctionDeclaration& decl
		) {
			const Shorthand s{ ctx };
			const SymID     self_sym  = decl.parameters.at(0).helios_symbol;
			const SymID     other_sym = decl.parameters.at(1).helios_symbol;
			auto            self      = [&] { return s.deref(s.ident(self_sym)); };
			auto            other     = [&] { return s.deref(s.ident(other_sym)); };

			switch (method.kind) {
			case Method::Kind::NotEqual:
				return s.unOp(
					code::BuiltinUnary::BooleanNot,
					code::resolveBinaryOperatorInGeneratedCode(ctx, equalOp(), self(), other())
				);
			case Method::Kind::Greater:
				return code::resolveBinaryOperatorInGeneratedCode(ctx, lessOp(), other(), self());
			case Method::Kind::LessEqual:
				return s.unOp(
					code::BuiltinUnary::BooleanNot,
					code::resolveBinaryOperatorInGeneratedCode(ctx, lessOp(), other(), self())
				);
			case Method::Kind::GreaterEqual:
				return s.unOp(
					code::BuiltinUnary::BooleanNot,
					code::resolveBinaryOperatorInGeneratedCode(ctx, lessOp(), self(), other())
				);
			case Method::Kind::Equal:
			case Method::Kind::Less: {
				std::vector<Box<code::Expr>> lhs_fields;
				std::vector<Box<code::Expr>> rhs_fields;
				for (const auto& field: method.owner_type.getInterface(ctx)->getFieldsView()) {
					lhs_fields.emplace_back(s.access(self(), field.getSymbol()));
					rhs_fields.emplace_back(s.access(other(), field.getSymbol()));
				}
				CORE_ASSERT(not lhs_fields.empty(), "A type carrying information has a field.");
				return method.kind == Method::Kind::Equal
				         ? generateEqual(ctx, lhs_fields, rhs_fields)
				         : generateLess(ctx, lhs_fields, rhs_fields);
			}
			default:
				CORE_UNREACHABLE();
			}
		}

		static PResult provide(Context& ctx, const QKey comparison_sym) {
			const auto* method = std::get_if<Method>(&getSymRef(comparison_sym)->other);
			CORE_ASSERT(
				method != nullptr and isComparisonMethodKind(method->kind),
				"QueryComparisonMethod expects a generated comparison symbol."
			);
			const auto& decl = ctx.query<QueryDeclOfFun>(comparison_sym)->valueOrThrow();

			const Shorthand              s{ ctx };
			std::vector<Box<code::Stmt>> body{};
			// The analyzer loses track of the `Box` moved into the statement: a false positive.
			// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
			body.emplace_back(s.ret(generateResult(ctx, *method, decl)));

			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryComparisonMethod);
}

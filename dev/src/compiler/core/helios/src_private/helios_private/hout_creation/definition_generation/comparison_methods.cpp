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
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <algorithm>
#include <ranges>
#include <vector>

namespace compiler::helios::defgen {
	using namespace code::shorthands;

	/**************
	|   SYMBOLS   |
	**************/

	const base::HashMap<ComparisonKind, base::StrID> COMPARISON_METHOD_SPELLING{
		{ ComparisonKind::Equal, base::StrID("==") },
		{ ComparisonKind::NotEqual, base::StrID("!=") },
		{ ComparisonKind::Lesser, base::StrID("<") },
		{ ComparisonKind::LesserEqual, base::StrID("<=") },
		{ ComparisonKind::Greater, base::StrID(">") },
		{ ComparisonKind::GreaterEqual, base::StrID(">=") },
	};

	const base::HashMap<ComparisonKind, Method::Kind> COMPARISON_DEFGEN_KIND{
		{ ComparisonKind::Equal, Method::Kind::Equal },
		{ ComparisonKind::NotEqual, Method::Kind::NotEqual },
		{ ComparisonKind::Lesser, Method::Kind::Lesser },
		{ ComparisonKind::LesserEqual, Method::Kind::LesserEqual },
		{ ComparisonKind::Greater, Method::Kind::Greater },
		{ ComparisonKind::GreaterEqual, Method::Kind::GreaterEqual },
	};

	const base::HashMap<ComparisonKind, tsh::MemberSpecialKind> COMPARISON_METHOD_SPECIAL_KIND{
		{ ComparisonKind::Equal, tsh::MemberSpecialKind::Equal },
		{ ComparisonKind::NotEqual, tsh::MemberSpecialKind::NotEqual },
		{ ComparisonKind::Lesser, tsh::MemberSpecialKind::Lesser },
		{ ComparisonKind::LesserEqual, tsh::MemberSpecialKind::LesserEqual },
		{ ComparisonKind::Greater, tsh::MemberSpecialKind::Greater },
		{ ComparisonKind::GreaterEqual, tsh::MemberSpecialKind::GreaterEqual },
	};

	const base::HashMap<Method::Kind, ComparisonKind> DEFGEN_KIND_TO_COMPARISON_KIND{
		{ Method::Kind::Equal, ComparisonKind::Equal },
		{ Method::Kind::NotEqual, ComparisonKind::NotEqual },
		{ Method::Kind::Lesser, ComparisonKind::Lesser },
		{ Method::Kind::LesserEqual, ComparisonKind::LesserEqual },
		{ Method::Kind::Greater, ComparisonKind::Greater },
		{ Method::Kind::GreaterEqual, ComparisonKind::GreaterEqual },
	};

	SymID comparisonSymForType(
		query::Context& ctx, const tsh::AbstractType type, const ComparisonKind kind
	) {
		const auto comparison
			= type.getInterface(ctx)->getSpecialElement(COMPARISON_METHOD_SPECIAL_KIND.at(kind));
		return comparison.value()->getSymbol();
	}

	SymID generatedComparisonSymForType(
		query::Context& ctx, const tsh::AbstractType type, const ComparisonKind kind
	) {
		return ctx.query<QueryGeneratedSymbol>({
			.name = base::StrID(COMPARISON_METHOD_SPELLING.at(kind)),
			.generated_symbol_data
			= Method{ .owner_type = type, .kind = COMPARISON_DEFGEN_KIND.at(kind) },
		});
	}

	/**************
	|   HELPERS   |
	**************/

	[[noreturn]] void comparisonNotYetImplemented(
		query::Context& ctx, const ComparisonKind kind, const tsh::AbstractType type
	) {
		ctx.log<dia::NotYetImplementedCodeError>(
			base::strConcat(
				"The `",
				COMPARISON_METHOD_SPELLING.at(kind),
				"` operator is not yet implemented for the type `",
				type.toString(),
				"`."
			),
			"Comparison operators are generated for numbers, characters, booleans, pointers, "
			"tuples and classes. Values of type `type` can only be compared for equality."
		);
		query::throwFailed();
		CORE_UNREACHABLE();
	}

	/**
	 * @brief Get the builtin operation corresponding to a comparison operator for a given type.
	 * @note Assumes that the types is a simple, data-carrying type. Panics otherwise.
	 */
	code::BuiltinBinary getComparisonBuiltinBinary(
		query::Context& ctx, const tsh::AbstractType type, const ComparisonKind comp_kind
	) {
		CORE_ASSERT(
			type.isSimple() && type.carriesInformation(ctx),
			"Type must be simple and carry information."
		);
		switch (type.getKind()) {
		case tsh::Kind::Byte:
		case tsh::Kind::Bool:
		case tsh::Kind::Char:
		case tsh::Kind::Integral:
		case tsh::Kind::RawPointer:
		case tsh::Kind::Pointer:
		case tsh::Kind::ManyPointer:
		case tsh::Kind::CPointer: {
			switch (comp_kind) {
			case ComparisonKind::Equal:
				return code::BuiltinBinary::IntegerEq;
			case ComparisonKind::NotEqual:
				return code::BuiltinBinary::IntegerNeq;
			case ComparisonKind::Lesser:
				return code::BuiltinBinary::IntegerLt;
			case ComparisonKind::LesserEqual:
				return code::BuiltinBinary::IntegerLteq;
			case ComparisonKind::Greater:
				return code::BuiltinBinary::IntegerGt;
			case ComparisonKind::GreaterEqual:
				return code::BuiltinBinary::IntegerGteq;
			default:
				CORE_UNREACHABLE();
			}
		}
		case tsh::Kind::Float: {
			switch (comp_kind) {
			case ComparisonKind::Equal:
				return code::BuiltinBinary::FloatEq;
			case ComparisonKind::NotEqual:
				return code::BuiltinBinary::FloatNeq;
			case ComparisonKind::Lesser:
				return code::BuiltinBinary::FloatLt;
			case ComparisonKind::LesserEqual:
				return code::BuiltinBinary::FloatLteq;
			case ComparisonKind::Greater:
				return code::BuiltinBinary::FloatGt;
			case ComparisonKind::GreaterEqual:
				return code::BuiltinBinary::FloatGteq;
			default:
				CORE_UNREACHABLE();
			}
		}
		case tsh::Kind::Meta: {
			switch (comp_kind) {
			case ComparisonKind::Equal:
				return code::BuiltinBinary::MetaEq;
			case ComparisonKind::NotEqual:
				return code::BuiltinBinary::MetaNeq;
			default:
				comparisonNotYetImplemented(ctx, comp_kind, tsh::getMetaType());
			}
		}
		default:
			comparisonNotYetImplemented(ctx, comp_kind, type);
		}
	}

	/**
	 * @brief Get the expression that compares two values, meant to be used for inline construction.
	 */
	Box<code::Expr> getSubComparison(
		query::Context& ctx, Box<code::Expr> lhs, Box<code::Expr> rhs, const ComparisonKind kind
	) {
		Shorthand s{ ctx };
		// Simple values are compared by value (dereferencing e.g. `box` or `ref` fields), others
		// are passed by reference to their comparison method.
		lhs             = s.prepToPassSelf(std::move(lhs));
		rhs             = s.prepToPassSelf(std::move(rhs));
		const auto type = lhs->expression_type.getType();
		if (type.isSimple()) {
			const auto operation = getComparisonBuiltinBinary(ctx, type, kind);
			return s.binOp(std::move(lhs), operation, std::move(rhs));
		} else {
			const auto comparison_sym = comparisonSymForType(ctx, type, kind);
			return s.call(s.ident(comparison_sym), std::move(lhs), std::move(rhs));
		}
	}

	/**
	 * @brief Access @p field of the parameter at @p param_idx of an aggregate comparison method.
	 * @note The access is cheap and free of side effects, so it is built anew for every use. A
	 * `ReusableExpr` would instead copy the bytes of the field, which is not allowed for values
	 * that are not trivially copyable.
	 */
	Box<code::Expr> fieldOf(
		const Shorthand& s, const HOUTFunctionDeclaration& decl, const usize param_idx, SymID field
	) {
		return s.access(s.deref(s.ident(decl.parameters.at(param_idx).helios_symbol)), field);
	}

	/***************
	|   BUILDERS   |
	***************/

	/**
	 * @brief Append statements that compare a simple type to a function body.
	 */
	void buildSimpleComparison(
		std::vector<Box<code::Stmt>>&  out_body,
		query::Context&                ctx,
		const HOUTFunctionDeclaration& decl,
		const ComparisonKind           kind
	) {
		const auto type = decl.parameters.at(0).type.getType();
		CORE_ASSERT(type.isSimple(), "Type must be simple.");
		Shorthand s{ ctx };

		// Information-less types are always equal.
		if (not type.carriesInformation(ctx)) {
			const bool accepts_equality = kind == ComparisonKind::Equal
			                           || kind == ComparisonKind::LesserEqual
			                           || kind == ComparisonKind::GreaterEqual;
			out_body.push_back(s.ret(s.litBool(accepts_equality)));
			return;
		}

		// Otherwise, compare normally.
		out_body.push_back(s.ret(getSubComparison(
			ctx,
			s.ident(decl.parameters.at(0).helios_symbol),
			s.ident(decl.parameters.at(1).helios_symbol),
			kind
		)));
	}

	/**
	 * @brief Compares two aggregate values for `==` or `!=` field by field,
	 * returning `true` or `false` as soon as the result is known.
	 * @param fields The fields to compare, in order.
	 */
	void buildAggregateEqComparison(
		std::vector<Box<code::Stmt>>&  out_body,
		query::Context&                ctx,
		const HOUTFunctionDeclaration& decl,
		const std::vector<SymID>&      fields,
		const ComparisonKind           kind
	) {
		const Shorthand s{ ctx };
		// Compare the fields in order, return if the comparison decides the result.
		for (const SymID field: fields)
			if (kind == ComparisonKind::Equal) {
				out_body.emplace_back(s.ifStmt(
					s.unOp(
						code::BuiltinUnary::BooleanNot,
						getSubComparison(
							ctx, fieldOf(s, decl, 0, field), fieldOf(s, decl, 1, field), kind
						)
					),
					{ s.ret(s.litBool(false)) }
				));
			} else if (kind == ComparisonKind::NotEqual) {
				out_body.emplace_back(s.ifStmt(
					getSubComparison(
						ctx, fieldOf(s, decl, 0, field), fieldOf(s, decl, 1, field), kind
					),
					{ s.ret(s.litBool(true)) }
				));
			} else
				CORE_UNREACHABLE();
		if (kind == ComparisonKind::Equal)
			out_body.emplace_back(s.ret(s.litBool(true)));
		else if (kind == ComparisonKind::NotEqual)
			out_body.emplace_back(s.ret(s.litBool(false)));
		else
			CORE_UNREACHABLE();
	}

	/**
	 * @brief Compares two aggregate values for `<`, `<=`, `>` or `>=` field by field,
	 * returning `true` or `false` as soon as the result is known.
	 * @param fields The fields to compare, in order.
	 */
	void buildAggregateOrdComparison(
		std::vector<Box<code::Stmt>>&  out_body,
		query::Context&                ctx,
		const HOUTFunctionDeclaration& decl,
		const std::vector<SymID>&      fields,
		const ComparisonKind           kind
	) {
		const Shorthand s{ ctx };
		const auto      strict_kind
			= kind == ComparisonKind::Lesser || kind == ComparisonKind::LesserEqual
		        ? ComparisonKind::Lesser
		        : ComparisonKind::Greater;
		const bool accept_equality = kind != strict_kind;

		// Compare the fields in order, return if the comparison decides the result.
		// There are two strict comparisons per pair of fields, one in each direction.
		for (const SymID field: fields) {
			out_body.emplace_back(s.ifStmt(
				getSubComparison(
					ctx, fieldOf(s, decl, 0, field), fieldOf(s, decl, 1, field), strict_kind
				),
				{ s.ret(s.litBool(true)) }
			));
			out_body.emplace_back(s.ifStmt(
				getSubComparison(
					ctx, fieldOf(s, decl, 1, field), fieldOf(s, decl, 0, field), strict_kind
				),
				{ s.ret(s.litBool(false)) }
			));
		}
		out_body.emplace_back(s.ret(s.litBool(accept_equality)));
	}

	void buildAggregateComparison(
		std::vector<Box<code::Stmt>>&  out_body,
		query::Context&                ctx,
		const HOUTFunctionDeclaration& decl,
		const ComparisonKind           kind
	) {
		const auto type = decl.parameters.at(0).type.getType();
		CORE_ASSERT(
			type.getKind() == tsh::Kind::Tuple || type.getKind() == tsh::Kind::Class,
			"Only tuples and classes are compared field by field."
		);

		// Collect the comparable fields: those which carry information.
		const auto fields = type.getInterface(ctx)->getFieldsView()
		                  | std::views::filter([&](const tsh::InterfaceElement& field) {
								return field.getType(ctx).getType().carriesInformation(ctx);
							})
		                  | std::views::transform(&tsh::InterfaceElement::getSymbol)
		                  | std::ranges::to<std::vector<SymID>>();

		// Build the body of the aggregate comparison.
		switch (kind) {
		case ComparisonKind::Equal:
		case ComparisonKind::NotEqual:
			buildAggregateEqComparison(out_body, ctx, decl, fields, kind);
			break;
		case ComparisonKind::Lesser:
		case ComparisonKind::LesserEqual:
		case ComparisonKind::Greater:
		case ComparisonKind::GreaterEqual:
			buildAggregateOrdComparison(out_body, ctx, decl, fields, kind);
			break;
		}
	}

	struct IMPLEMENT_QUERY(QueryGeneratedComparisonMethod, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey comparison_sym) {
			const auto method      = std::get<Method>(getSymRef(comparison_sym)->other);
			const auto owner_type  = method.owner_type;
			const auto defgen_kind = method.kind;
			const auto comp_kind   = DEFGEN_KIND_TO_COMPARISON_KIND.at(defgen_kind);
			const auto decl        = ctx.query<QueryDeclOfFun>(comparison_sym);

			std::vector<Box<code::Stmt>> body{};

			if (owner_type.isSimple())
				buildSimpleComparison(body, ctx, decl->valueOrThrow(), comp_kind);
			else if (owner_type.getKind() == tsh::Kind::Tuple
			         || owner_type.getKind() == tsh::Kind::Class)
				buildAggregateComparison(body, ctx, decl->valueOrThrow(), comp_kind);
			else
				// Other kinds (e.g. static arrays or variants) have no fields to compare.
				comparisonNotYetImplemented(ctx, comp_kind, owner_type);

			return HOUTFunction(
				code::generatedOrigin(),
				&decl->valueOrThrow(),
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryGeneratedComparisonMethod);
}

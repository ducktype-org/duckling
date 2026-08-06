#include "copy_constructors.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/copy_constructor.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/hout_creation/definition_generation/copy_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/length_methods.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	using namespace code::shorthands;

	bool isUserDefinedCopyConstructor(query::Context& ctx, const SymID sym) {
		if (kind(sym) != SymbolKind::Constructor) return false;
		const auto maybe_pst = maybeSymbolPst(sym);
		CORE_ASSERT(maybe_pst.has_value(), "User defined copy constructor without a PST element");
		const auto pst = maybe_pst.value();
		return pst.unlock(ctx).dynamicCast<pst::CopyConstructor>().has_value();
	}

	base::Optional<SymID> userCopyConstructorOf(query::Context& ctx, const SymID class_sym) {
		if (kind(class_sym) != SymbolKind::Class) return {};
		const auto& class_data = ctx.query<QueryClassSymbolData>(class_sym)->valueOrThrow();
		for (const SymID ctor: class_data.constructors)
			if (isUserDefinedCopyConstructor(ctx, ctor)) return ctor;
		return {};
	}

	SymID copyConstructorSymForType(query::Context& ctx, const tsh::AbstractType type) {
		// First, try to get the user-defined constructor.
		if (type.getKind() == tsh::Kind::Class) {
			if (const auto user
			    = userCopyConstructorOf(ctx, type.as<tsh::ClassAbstractType>().getSymbol()))
				return user.value();
		}

		// Otherwise, we use the default one.
		return ctx.query<QueryGeneratedSymbol>({
			.name                  = base::StrID("__copy"),
			.generated_symbol_data = Constructor{ .type = type, .kind = Constructor::Kind::Copy },
		});
	}

	namespace {
		// Builds the copy-constructor body for a class or tuple.
		std::vector<Box<code::Stmt>> buildAggregateCopyBody(
			query::Context& ctx, const tsh::AbstractType& owner_type, const SymID source_symbol
		) {
			const std::vector<tsh::InterfaceElement> fields
				= owner_type.getInterface(ctx)->getFieldsView() | std::ranges::to<std::vector>();

			const Shorthand s{ ctx };

			// One value per field, in declaration order: a copy of the source's field.
			std::vector<Box<code::Expr>> field_values;
			field_values.reserve(fields.size());
			for (const auto& field: fields)
				field_values.emplace_back(
					s.copy(s.access(s.deref(s.ident(source_symbol)), field.getSymbol()))
				);

			// The whole value is built in place by a single expression:
			// `return create_aggregate(T) { <copy of (*source).field>... };`
			std::vector<Box<code::Stmt>> body;
			body.emplace_back(s.ret(s.createAggregate(owner_type, std::move(field_values))));

			return body;
		}

		std::vector<Box<code::Stmt>> buildStaticArrayCopyBody(
			query::Context&                     ctx,
			const tsh::StaticArrayAbstractType& array_type,
			const SymID                         copy_sym,
			const SymID                         source_symbol
		) {
			using Variable = GeneratedFunctionVariable;
			using enum code::BuiltinBinary;

			std::vector<Box<code::Stmt>> body;

			const Shorthand s{ ctx };

			if (array_type.getSize() == 0) {
				// There is no element to copy, so there is nothing for an aggregate to build.
				// `return zero_initialized T[0];`
				body.emplace_back(s.ret(s.defaultValue(array_type)));
				return body;
			}

			const auto u64_abs_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned);
			const auto u64_type = tsh::SymbolType<>{ u64_abs_type,
				                                     tsh::ReferenceKind::Direct,
				                                     tsh::Mutability::Mutable };

			// The elements of the aggregate differ only in the index they copy, so the whole array
			// is built by a single value that reads the index off a variable, advanced once per
			// element by the per-element statements.
			// var __i: u64 = 0;
			const SymID i_sym    = ctx.query<QueryGeneratedSymbol>({
				   .name = base::StrID("__i"),
				   .generated_symbol_data
                = Variable{ .function_symbol = copy_sym, .variable_index = 0, .type = u64_type },
            });
			auto        zero_val = numeric_value::NumericValue::createOfType(u64_abs_type)
			                    .expect("u64 creation failed");
			body.emplace_back(s.var(i_sym, u64_type, s.litNum(zero_val)));

			auto one_val = numeric_value::NumericValue::createOfType(u64_abs_type, 1)
			                   .expect("u64 creation failed");

			std::vector<Box<code::Expr>> element_values;
			element_values.emplace_back(
				s.copy(s.index(s.deref(s.ident(source_symbol)), s.ident(i_sym)))
			);

			// return create_aggregate(T[N]) [ <copy of (*source)[__i]> ] per_element { __i = __i + 1; };
			body.emplace_back(s.ret(s.createAggregate(
				array_type,
				std::move(element_values),
				{
					s.assign(s.ident(i_sym), s.binOp(s.ident(i_sym), IntegerAdd, s.litNum(one_val))),
				}
			)));

			return body;
		}

		std::vector<Box<code::Stmt>> buildDynamicArrayCopyBody(
			query::Context&                      ctx,
			const tsh::DynamicArrayAbstractType& array_type,
			const SymID                          copy_sym,
			const SymID                          source_symbol,
			const tsh::SymbolType<>&             result_symbol_type
		) {
			using Variable = GeneratedFunctionVariable;

			std::vector<Box<code::Stmt>> body;

			const Shorthand s{ ctx };

			// var __result: List[T] = <zero>;
			const SymID res_sym = ctx.query<QueryGeneratedSymbol>({
				.name                  = base::StrID("__result"),
				.generated_symbol_data = Variable{ .function_symbol = copy_sym,
			                                       .variable_index  = 0,
			                                       .type            = result_symbol_type },
			});
			body.emplace_back(
				s.var(res_sym, result_symbol_type, s.defaultValue(result_symbol_type.getType()))
			);

			using enum code::BuiltinBinary;

			const auto u64_abs_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned);
			const auto u64_type = tsh::SymbolType<>{ u64_abs_type,
				                                     tsh::ReferenceKind::Direct,
				                                     tsh::Mutability::Mutable };

			// var __i: u64 = 0;
			const SymID i_sym    = ctx.query<QueryGeneratedSymbol>({
				   .name = base::StrID("__i"),
				   .generated_symbol_data
                = Variable{ .function_symbol = copy_sym, .variable_index = 1, .type = u64_type },
            });
			auto        zero_val = numeric_value::NumericValue::createOfType(u64_abs_type)
			                    .expect("u64 creation failed");
			body.emplace_back(s.var(i_sym, u64_type, s.litNum(zero_val)));

			const SymID length_method_sym = defgen::lengthMethodForType(ctx, array_type);
			auto        one_val = numeric_value::NumericValue::createOfType(u64_abs_type, 1)
			                   .expect("u64 creation failed");

			// while (__i < source.length()) { __result += <copy of (*source)[__i]>; __i = __i + 1; }
			body.emplace_back(s.whileStmt(
				s.binOp(
					s.ident(i_sym),
					IntegerLt,
					s.call(s.ident(length_method_sym), s.ident(source_symbol))
				),
				{
					s.expr(s.listPush(
						s.ident(res_sym),
						s.copy(s.index(s.deref(s.ident(source_symbol)), s.ident(i_sym)))
					)),
					s.assign(s.ident(i_sym), s.binOp(s.ident(i_sym), IntegerAdd, s.litNum(one_val))),
				}
			));

			// return __result;
			body.emplace_back(s.ret(s.ident(res_sym)));

			return body;
		}
	}

	struct IMPLEMENT_QUERY(QueryDefaultCopyConstructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey owner_type) {
			const SymID copy_sym           = copyConstructorSymForType(ctx, owner_type);
			const auto& cctor_decl         = ctx.query<QueryDeclOfFun>(copy_sym)->valueOrThrow();
			const SymID source_symbol      = cctor_decl.parameters.at(0).helios_symbol;
			const auto  result_symbol_type = cctor_decl.return_type;

			std::vector<Box<code::Stmt>> body;
			switch (owner_type.getKind()) {
			case tsh::Kind::Class:
			case tsh::Kind::Tuple:
				body = buildAggregateCopyBody(ctx, owner_type, source_symbol);
				break;
			case tsh::Kind::StaticArray:
				body = buildStaticArrayCopyBody(
					ctx, owner_type.as<tsh::StaticArrayAbstractType>(), copy_sym, source_symbol
				);
				break;
			case tsh::Kind::DynamicArray:
				body = buildDynamicArrayCopyBody(
					ctx,
					owner_type.as<tsh::DynamicArrayAbstractType>(),
					copy_sym,
					source_symbol,
					result_symbol_type
				);
				break;
			default:
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"Default copy constructor for type `", owner_type.toString(), "`."
					),
					""
				));
				return query::Failed();
			}

			return HOUTFunction(
				code::generatedOrigin(),
				&cctor_decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDefaultCopyConstructor);
}

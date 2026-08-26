#include "copy_constructors.hpp"

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

#include <diagnostic/placeholder.hpp>
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
					s.copyValue(s.access(s.deref(s.ident(source_symbol)), field.getSymbol()))
				);

			// The whole value is built in place by a single expression:
			// `return create_aggregate(T) { <copy of (*source).field>... };`
			std::vector<Box<code::Stmt>> body;
			body.emplace_back(s.ret(s.createAggregate(owner_type, std::move(field_values))));

			return body;
		}

		/**
		 * @brief Builds the copy-constructor body for a variant: copies the active alternative.
		 *
		 * Every alternative gets a case, so the match is exhaustive without a wildcard. Each case
		 * binds the payload and rebuilds the variant around a copy of it, which keeps the tag of
		 * the source.
		 */
		std::vector<Box<code::Stmt>> buildVariantCopyBody(
			query::Context&                 ctx,
			const tsh::VariantAbstractType& variant_type,
			const SymID                     copy_sym,
			const SymID                     source_symbol,
			const tsh::SymbolType<>&        result_symbol_type
		) {
			using Variable = GeneratedFunctionVariable;

			const Shorthand s{ ctx };
			const auto&     alternatives = variant_type.getUnderlyingTypes();

			std::vector<code::MatchExpr::Case> cases;
			for (usize i = 0; i < alternatives.size(); i++) {
				// A binding is always a reference to the payload, which for a reference-like
				// alternative is the stored reference itself.
				const auto payload_type = alternatives[i]
				                              .withReferenceKind(tsh::ReferenceKind::Ref)
				                              .withMutability(tsh::Mutability::Mutable);

				const SymID payload_sym = ctx.query<QueryGeneratedSymbol>({
					.name                  = base::StrID(base::strConcat("__alternative_", i)),
					.generated_symbol_data = Variable{ .function_symbol = copy_sym,
				                                       .variable_index  = i,
				                                       .type            = payload_type },
				});

				auto copy_through = [&](const tsh::SymbolType<>& value_type) -> Box<code::Expr> {
					if (value_type.isTriviallyCopyable(ctx)) return s.deref(s.ident(payload_sym));
					return s.call(
						s.ident(copyConstructorSymForType(ctx, value_type.getType())),
						s.ident(payload_sym)
					);
				};

				auto payload_value = [&]() -> Box<code::Expr> {
					switch (alternatives[i].getRefKind()) {
					case tsh::ReferenceKind::Direct:
						return copy_through(alternatives[i]);
					case tsh::ReferenceKind::Ref:
						// Copying a reference copies the reference, and the binding already is it.
						return s.ident(payload_sym);
					case tsh::ReferenceKind::Box:
						// A box is deep-copied into a fresh allocation holding a copy of the
						// pointee.
						return makeBoxAllocCall(
							ctx,
							code::generatedOrigin(),
							copy_through(alternatives[i].getPointeeSymbolType())
						);
					}
					CORE_UNREACHABLE();
				}();

				cases.emplace_back(Shorthand::matchCase(
					i,
					payload_sym,
					makeBox<code::VariantConstructExpr>(
						ctx, code::generatedOrigin(), std::move(payload_value), result_symbol_type, i
					)
				));
			}

			// `source` is already a reference to the variant, which is what the match wants.
			std::vector<Box<code::Stmt>> body;
			body.emplace_back(s.ret(s.matchExpr(s.ident(source_symbol), std::move(cases))));
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

			const auto i64_abs_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed);
			const auto i64_type = tsh::SymbolType<>{ i64_abs_type,
				                                     tsh::ReferenceKind::Direct,
				                                     tsh::Mutability::Mutable };

			// The elements of the aggregate differ only in the index they copy, so the whole array
			// is built by a single value that reads the index off a variable, advanced once per
			// element by the per-element statements.
			// var __i: i64 = 0;
			const SymID i_sym    = ctx.query<QueryGeneratedSymbol>({
				   .name = base::StrID("__i"),
				   .generated_symbol_data
                = Variable{ .function_symbol = copy_sym, .variable_index = 0, .type = i64_type },
            });
			auto        zero_val = numeric_value::NumericValue::createOfType(i64_abs_type)
			                    .expect("i64 creation failed");
			body.emplace_back(s.var(i_sym, i64_type, s.litNum(zero_val)));

			auto one_val = numeric_value::NumericValue::createOfType(i64_abs_type, 1)
			                   .expect("i64 creation failed");

			std::vector<Box<code::Expr>> element_values;
			element_values.emplace_back(
				s.copyValue(s.index(s.deref(s.ident(source_symbol)), s.ident(i_sym)))
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
			case tsh::Kind::Variant:
				body = buildVariantCopyBody(
					ctx,
					owner_type.as<tsh::VariantAbstractType>(),
					copy_sym,
					source_symbol,
					result_symbol_type
				);
				break;
			default:
				ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
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

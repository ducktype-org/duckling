#include "default_destructors.hpp"

#include <helios/attributes/builtins.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios_private/hout_creation/definition_generation/length_methods.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <ranges>

namespace compiler::helios::defgen {
	using namespace code::shorthands;

	SymID destructSymForType(query::Context& ctx, const tsh::AbstractType type) {
		return ctx.query<QueryGeneratedSymbol>({
			.name = base::StrID("__destruct"),
			.generated_symbol_data
			= Method{ .owner_type = type, .kind = Method::Kind::DefaultDestructor },
		});
	}

	base::Optional<SymID> destructSymForSymbolType(query::Context& ctx, tsh::SymbolType<> type) {
		if (type.getRefKind() == tsh::ReferenceKind::Box)
			return boxDestructorSymForType(ctx, type.getType());
		if (type.getRefKind() == tsh::ReferenceKind::Ref) return {};
		return destructSymForType(ctx, type.getType());
	}

	bool isUserDefinedDestructor(query::Context&, const SymID sym) {
		return kind(sym) == SymbolKind::Destructor;
	}

	base::Optional<SymID> userDestructorOf(query::Context& ctx, const SymID class_sym) {
		if (kind(class_sym) != SymbolKind::Class) return {};
		return ctx.query<QueryClassSymbolData>(class_sym)->valueOrThrow().destructor;
	}

	namespace {
		/**
		 * @brief Append the statements that destroy the `location` value.
		 *
		 * - Trivially-destructible values do nothing.
		 * - A `box T` is destroyed by calling its `box_destructor` builtin (which destroys the
		 *   pointee and then frees the heap storage).
		 * - Non-trivially-destructible class, static-array, tuple and dynamic-array members are
		 *   destroyed by calling their own destructor with a reference to `location`.
		 */
		void appendDestruction(
			query::Context& ctx, std::vector<Box<code::Stmt>>& body, Box<code::Expr> location
		) {
			const tsh::SymbolType<> type = location->expression_type.getSymbolType();

			if (type.isTriviallyDestructible(ctx)) return;

			const Shorthand s{ ctx };

			// A `box T` owns its pointee and its heap storage. Its `box_destructor` builtin destroys
			// the pointee and then frees the memory, so we just call it with the box by value.
			if (type.getRefKind() == tsh::ReferenceKind::Box) {
				const auto pointee_type = type.getType();

				appendDestruction(ctx, body, s.deref(location->clone()));

				body.emplace_back(s.expr(
					s.call(s.ident(boxDestructorSymForType(ctx, pointee_type)), std::move(location))
				));
				return;
			}

			// Now we have a direct value which should be destroyed by calling its destructor.
			const auto abstract_type = type.getType();
			CORE_ASSERT(
				abstract_type.getKind() == tsh::Kind::Class
					or abstract_type.getKind() == tsh::Kind::StaticArray
					or abstract_type.getKind() == tsh::Kind::Tuple
					or abstract_type.getKind() == tsh::Kind::DynamicArray
					or abstract_type.getKind() == tsh::Kind::String
					or abstract_type.getKind() == tsh::Kind::Variant,
				"Tried to generate a destructor call for a type which shouldn't need one"
			);

			const SymID dtor_sym = destructSymForType(ctx, abstract_type);
			body.emplace_back(s.expr(s.call(s.ident(dtor_sym), s.refOf(std::move(location)))));
		}

		/**
		 * @brief Build `(*self).<member>` - a dereference of the `self` followed by a
		 * field access.
		 */
		Box<code::Expr> derefSelfField(query::Context& ctx, SymID self_symbol, SymID field) {
			const Shorthand s{ ctx };
			return s.access(s.deref(s.ident(self_symbol)), field);
		}

		/**
		 * @brief Builds the destructor body for a class or tuple. Members are destroyed in reverse
		 * declaration order. For a class with a user-defined destructor, the user code runs first.
		 */
		std::vector<Box<code::Stmt>> buildAggregateDestructBody(
			query::Context& ctx, const tsh::AbstractType& owner_type, const SymID self_symbol
		) {
			std::vector<Box<code::Stmt>> body;

			const Shorthand s{ ctx };

			// For a class that declares its own destructor, run the user code before destroying the
			// members.
			if (owner_type.getKind() == tsh::Kind::Class) {
				if (const auto user
				    = userDestructorOf(ctx, owner_type.as<tsh::ClassAbstractType>().getSymbol())) {
					body.emplace_back(s.expr(s.call(s.ident(user.value()), s.ident(self_symbol))));
				}
			}

			const std::vector<tsh::InterfaceElement> fields
				= owner_type.getInterface(ctx)->getFieldsView() | std::ranges::to<std::vector>();

			// (*self).field.__destruct(...) for each non-trivial field, in reverse.
			for (const auto& field: std::views::reverse(fields))
				appendDestruction(ctx, body, derefSelfField(ctx, self_symbol, field.getSymbol()));

			return body;
		}

		// `var __i: u64 = 0;`
		SymID buildLoopCounter(
			query::Context& ctx, std::vector<Box<code::Stmt>>& body, SymID dtor_sym
		) {
			using Variable = GeneratedFunctionVariable;

			const auto u64_abs_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned);
			const auto u64_type = tsh::SymbolType<>::withDefaults(u64_abs_type);

			const SymID i_sym    = ctx.query<QueryGeneratedSymbol>({
				   .name = base::StrID("__i"),
				   .generated_symbol_data
                = Variable{ .function_symbol = dtor_sym, .variable_index = 0, .type = u64_type },
            });
			auto        zero_val = numeric_value::NumericValue::createOfType(u64_abs_type)
			                    .expect("u64 creation failed");

			const Shorthand s{ ctx };
			body.emplace_back(s.var(i_sym, u64_type, s.litNum(zero_val)));
			return i_sym;
		}

		// `__i = __i + 1;`.
		Box<code::Stmt> buildLoopIncrement(query::Context& ctx, SymID i_sym) {
			const auto u64_abs_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned);
			auto one_val = numeric_value::NumericValue::createOfType(u64_abs_type, 1)
			                   .expect("u64 creation failed");
			const Shorthand s{ ctx };
			return s.assign(
				s.ident(i_sym),
				s.binOp(s.ident(i_sym), code::BuiltinBinary::IntegerAdd, s.litNum(one_val))
			);
		}

		std::vector<Box<code::Stmt>> buildStaticArrayDestructBody(
			query::Context&                     ctx,
			const tsh::StaticArrayAbstractType& array_type,
			const SymID                         dtor_sym,
			const SymID                         self_symbol
		) {
			std::vector<Box<code::Stmt>> body;

			const usize size = array_type.getSize();

			// Nothing to destroy for empty or trivially-destructible arrays.
			if (size == 0 || array_type.getElementType().isTriviallyDestructible(ctx)) return body;

			// var __i: u64 = 0;
			const SymID i_sym = buildLoopCounter(ctx, body, dtor_sym);

			// while (__i < size) {
			// 		(*self)[__i].__destruct(...);
			// 		__i = __i + 1;
			// }
			const auto u64_abs_type
				= tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned);
			auto size_val = numeric_value::NumericValue::createOfType(u64_abs_type, size)
			                    .expect("u64 creation failed");

			const Shorthand s{ ctx };
			auto            condition
				= s.binOp(s.ident(i_sym), code::BuiltinBinary::IntegerLt, s.litNum(size_val));

			std::vector<Box<code::Stmt>> loop_body;
			appendDestruction(
				ctx, loop_body, s.index(s.deref(s.ident(self_symbol)), s.ident(i_sym))
			);
			loop_body.emplace_back(buildLoopIncrement(ctx, i_sym));

			body.emplace_back(s.whileStmt(std::move(condition), std::move(loop_body)));

			return body;
		}

		std::vector<Box<code::Stmt>> buildDynamicArrayDestructBody(
			query::Context&                      ctx,
			const tsh::DynamicArrayAbstractType& array_type,
			const SymID                          dtor_sym,
			const SymID                          self_symbol
		) {
			std::vector<Box<code::Stmt>> body;

			const Shorthand s{ ctx };

			// Destroy each element only if the element type is not trivially destructible.
			// while (__i < self.length()) { (*self)[__i].__destruct(...); __i = __i + 1; }
			if (not array_type.getElementType().isTriviallyDestructible(ctx)) {
				// var __i: u64 = 0;
				const SymID i_sym             = buildLoopCounter(ctx, body, dtor_sym);
				const SymID length_method_sym = defgen::lengthMethodForType(ctx, array_type);

				Box<code::Expr> len_expr = s.call(s.ident(length_method_sym), s.ident(self_symbol));
				auto            condition
					= s.binOp(s.ident(i_sym), code::BuiltinBinary::IntegerLt, std::move(len_expr));

				std::vector<Box<code::Stmt>> loop_body;
				appendDestruction(
					ctx, loop_body, s.index(s.deref(s.ident(self_symbol)), s.ident(i_sym))
				);
				loop_body.emplace_back(buildLoopIncrement(ctx, i_sym));

				body.emplace_back(s.whileStmt(std::move(condition), std::move(loop_body)));
			}

			body.emplace_back(s.expr(s.call(
				s.ident(listFreeSymForType(ctx, array_type.getElementType().getType())),
				s.move(s.ident(self_symbol))
			)));

			return body;
		}
	}

	struct IMPLEMENT_QUERY(QueryDefaultDestructor, query::QResult<HOUTFunction>) {
		static PResult provide(Context& ctx, const QKey owner_type) {
			const SymID dtor_sym    = destructSymForType(ctx, owner_type);
			const auto& dtor_decl   = ctx.query<QueryDeclOfFun>(dtor_sym)->valueOrThrow();
			const SymID self_symbol = dtor_decl.parameters.at(0).helios_symbol;

			std::vector<Box<code::Stmt>> body;
			switch (owner_type.getKind()) {
			case tsh::Kind::Class:
			case tsh::Kind::Tuple:
				body = buildAggregateDestructBody(ctx, owner_type, self_symbol);
				break;
			case tsh::Kind::StaticArray:
				body = buildStaticArrayDestructBody(
					ctx, owner_type.as<tsh::StaticArrayAbstractType>(), dtor_sym, self_symbol
				);
				break;
			case tsh::Kind::DynamicArray:
				body = buildDynamicArrayDestructBody(
					ctx, owner_type.as<tsh::DynamicArrayAbstractType>(), dtor_sym, self_symbol
				);
				break;
			default:
				// Other types (e.g. strings and variants) either have a no-op destructor or their
				// destruction is not yet implemented. This stub just provides an empty destructor.
				break;
			}

			return HOUTFunction(
				code::generatedOrigin(),
				&dtor_decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDefaultDestructor);

	HOUTFunction buildBoxDestructor(query::Context& ctx, const tsh::AbstractType pointee_type) {
		const SymID box_dtor_sym = boxDestructorSymForType(ctx, pointee_type);
		const auto& dtor_decl    = ctx.query<QueryDeclOfFun>(box_dtor_sym)->valueOrThrow();
		const SymID self_symbol  = dtor_decl.parameters.at(0).helios_symbol;

		const Shorthand s{ ctx };

		std::vector<Box<code::Stmt>> body;

		// Destroy the pointee: `appendDestruction(*self)`.
		appendDestruction(ctx, body, s.deref(s.ident(self_symbol)));

		body.emplace_back(s.expr(
			s.call(s.ident(boxFreeSymForType(ctx, pointee_type)), s.move(s.ident(self_symbol)))
		));

		return HOUTFunction(
			code::generatedOrigin(),
			&dtor_decl,
			std::make_shared<const code::CodeBlock>(code::CodeBlock{
				.statements = std::move(body),
			})
		);
	}
}

#include "function_queries.hpp"

#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/return.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/copy_constructor.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/method.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/symbols/query_class_of_member.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/errors/errors.hpp>
#include <helios_private/hout_creation/definition_generation/class_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_copy_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_destructors.hpp>
#include <helios_private/hout_creation/definition_generation/length_methods.hpp>
#include <helios_private/hout_creation/definition_generation/list_methods.hpp>
#include <helios_private/hout_creation/definition_generation/to_string_methods.hpp>
#include <helios_private/hout_creation/definition_generation/tuple_constructor.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/hout_stmt_compilation.hpp>
#include <helios_private/pst_layer/stmts_from_aggregate.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	struct IMPLEMENT_QUERY(QueryCodeOfFun, query::QResult<HOUTFunction>) {
		/**
		 * Creates HOUT code block from single-statement function body.
		 * i.e.: handles the `fun abc() = expr;` case.
		 * Should only be used for the whole function body.
		 */
		static code::CodeBlock queryCodeOfSingleStmtFunctionBody(
			query::Context&                   ctx,
			pst::Access<pst::CodeBlockOrStmt> body,
			tsh::SymbolType<>                 return_type
		) {
			CORE_ASSERT(body->getType() == pst::CodeBlockOrStmt::Type::SingleStmt, "Bad body type!");

			code::CodeBlock block({});

			auto stmt    = body->getStmt().unlock(ctx);
			auto as_expr = stmt.dynamicCast<pst::ExprStmt>();
			if (as_expr) {
				auto expr_coerced
					= getHoutOfExprWithExpectedType(
						  ctx, as_expr.value()->getExpr().unlock(ctx)->getExpr(), return_type
					)
				          .valueOrThrow();

				block.statements.emplace_back(makeBox<code::ReturnStmt>(
					expr_coerced->origin.generatedFrom(), std::move(expr_coerced)
				));
				return block;
			} else {
				ctx.logInt(makeBox<SingleStmtFunctionMustBeExprError>(stmt->getStablePosition()));
				CORE_PANIC("Not handling errors here yet... (single stmt function body)");
			}
		}

		struct HOUTFunctionMaker final: public pst::PstVisitorPanicky {
			query::Context& ctx;
			SymID           original_symbol;

			base::Optional<HOUTFunction> out;

			HOUTFunctionMaker(query::Context& ctx, SymID symbol):
				  ctx(ctx),
				  original_symbol(symbol) {}

			std::shared_ptr<const code::CodeBlock> processBody(
				const HOUTFunctionDeclaration& decl, pst::AccessLocked<pst::CodeBlockOrStmt> body
			) {
				std::shared_ptr<const code::CodeBlock> output_body = nullptr;

				if (body.unlock(ctx)->getType() == pst::CodeBlockOrStmt::Type::SingleStmt) {
					// The `fun abc() = expr;` case.

					code::CodeBlock function_body
						= queryCodeOfSingleStmtFunctionBody(ctx, body.unlock(ctx), decl.return_type);
					output_body = std::make_shared<const code::CodeBlock>(std::move(function_body));
				} else {
					CORE_ASSERT(
						body.unlock(ctx)->getType() == pst::CodeBlockOrStmt::Type::CodeBlock,
						"This should not happen"
					);
					output_body = compileCodeOfCodeBlock(ctx, body, decl.return_type);
				}
				CORE_ASSERT(output_body != nullptr, "Function declaration must be present here");

				return output_body;
			}

			void visitFun(pst::Access<pst::Fun> stmt) final {
				// declaration:
				auto& decl = ctx.query<QueryDeclOfFun>(original_symbol)->valueOrThrow();
				// body:
				auto fun_body    = stmt->getBody();
				auto output_body = processBody(decl, fun_body);

				this->out.emplace(HOUTFunction(code::pstOrigin(stmt), &decl, output_body));
			}

			void visitMethod(pst::Access<pst::Method> stmt) final {
				// declaration:
				auto& decl = ctx.query<QueryDeclOfFun>(original_symbol)->valueOrThrow();

				// body:
				auto fun_body    = stmt->getBody();
				auto output_body = processBody(decl, fun_body);

				this->out.emplace(HOUTFunction(code::pstOrigin(stmt), &decl, output_body));
			}

			void visitCopyConstructor(pst::Access<pst::CopyConstructor> stmt) final {
				// declaration:
				auto& decl = ctx.query<QueryDeclOfFun>(original_symbol)->valueOrThrow();
				validateConstructorSource(stmt, decl);

				// body:
				auto output_body = processBody(decl, stmt->getBody());
				this->out.emplace(HOUTFunction(code::pstOrigin(stmt), &decl, output_body));
			}

			// Validates a user-defined copy/move constructor's source parameter. The copy
			// constructor must declare exactly one parameter, which must be a reference
			// (`ref`/`const ref`) to its own class.
			template<class ConstructorElement>
			void validateConstructorSource(
				pst::Access<ConstructorElement> stmt, const HOUTFunctionDeclaration& decl
			) {
				const auto class_type
					= ctx.query<QueryClassOfMember>(original_symbol)->valueOrThrow();

				const auto params_source = stmt->getParams().unlock(ctx)->getStablePosition();

				if (decl.parameters.size() != 1) {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						"A copy constructor must declare exactly one parameter: a reference to "
						"the object being copied.",
						params_source
					));
					query::throwFailed();
				}

				const auto other_type   = decl.parameters.at(0).type;
				const bool is_reference = other_type.getRefKind() == tsh::ReferenceKind::Ref;
				const bool is_matching_class
					= other_type.getType().getKind() == tsh::Kind::Class
				   && other_type.getType().as<tsh::ClassAbstractType>().getSymbol()
				          == class_type.getSymbol();
				if (!is_reference || !is_matching_class) {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"A copy constructor's parameter must be a constant reference to its "
							"own "
							"class `",
							name(class_type.getSymbol()),
							"`."
						),
						params_source
					));
					query::throwFailed();
				}
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			const auto& sym_ref = getSymRef(key);
			// Non-generated symbol data.
			if (sym_ref->getPSTDataOpt().has_value()) {
				CORE_ASSERT(
					isFunctionLike(kind(key)),
					"Function creation called on non-function, non-method and non-constructor "
					"symbol"
				);
				HOUTFunctionMaker func_maker(ctx, key);
				stmt(ctx, key).value()->acceptVisitor(func_maker);

				return func_maker.out.value();
			}


			// Generated symbol data.
			variant_match(sym_ref->other) {
				variant_case(defgen::GeneratedSymbolData, gsd_data) {
					variant_match(gsd_data.data) {
						variant_case(defgen::GeneratedSymbolData::ImplicitConstructor, ctor) {
							const auto& type = ctor.target_type;
							if (type.getKind() == tsh::Kind::Class) {
								const auto& class_type = type.as<tsh::ClassAbstractType>();
								return ctx.query<defgen::QueryImplicitClassConstructor>(class_type)
								    ->valueOrThrow();
							} else if (type.getKind() == tsh::Kind::Tuple) {
								const auto& tuple_type = type.as<tsh::TupleAbstractType>();
								return ctx.query<defgen::QueryTuplePackConstructor>(tuple_type)
								    ->valueOrThrow();
							} else {
								CORE_UNREACHABLE();
							}
						}
						variant_case(defgen::GeneratedSymbolData::DefaultClassConstructor, ctor) {
							const auto& type = ctx.query<QueryTypeFromDefinition>(ctor.class_symbol)
							                       ->valueOrThrow()
							                       .getType()
							                       .as<tsh::ClassAbstractType>();
							return ctx.query<defgen::QueryDefaultClassConstructor>(type)
							    ->valueOrThrow();
						}
						variant_case(defgen::GeneratedSymbolData::DefaultDestructor, dtor) {
							return ctx.query<defgen::QueryDefaultDestructor>(dtor.owner_type)
							    ->valueOrThrow();
						}
						variant_case(defgen::GeneratedSymbolData::DefaultCopyConstructor, cctor) {
							return ctx.query<defgen::QueryDefaultCopyConstructor>(cctor.owner_type)
							    ->valueOrThrow();
						}
						variant_case(
							defgen::GeneratedSymbolData::DefaultStaticArrayConstructor, ctor
						) {
							return ctx
							    .query<defgen::QueryDefaultStaticArrayConstructor>(ctor.array_type)
							    ->valueOrThrow();
						}
						variant_case(defgen::GeneratedSymbolData::ToStringMethod, to_string) {
							return ctx.query<defgen::QueryToStringMethod>(to_string.owner_type)
							    ->valueOrThrow();
						}
						variant_case(defgen::GeneratedSymbolData::LengthMethod, length_method) {
							return ctx.query<defgen::QueryLengthMethod>(length_method.owner_type)
							    ->valueOrThrow();
						}
						variant_case(defgen::GeneratedSymbolData::PushMethod, push_method) {
							return ctx.query<defgen::QueryPushMethod>(push_method.owner_type)
							    ->valueOrThrow();
						}
						variant_case(defgen::GeneratedSymbolData::PopMethod, pop_method) {
							return ctx.query<defgen::QueryPopMethod>(pop_method.owner_type)
							    ->valueOrThrow();
						}
						variant_default {
							CORE_PANIC(base::strConcat(
								"QueryTypeOfSymbol: Generated symbol '",
								prettyDebugPrint(key, ctx),
								"' not supported"
							));
						}
					}
				}
				variant_default {
					CORE_PANIC("QueryCodeOfFun: Symbol is neither PST nor Generated");
				}
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCodeOfFun);
}

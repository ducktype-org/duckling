// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "function_queries.hpp"

#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/return.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/copy_constructor.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/destructor.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/method.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/repl_utils/repl_queries.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/errors/errors.hpp>
#include <helios_private/hout_creation/definition_generation/class_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/copy_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_destructors.hpp>
#include <helios_private/hout_creation/definition_generation/length_methods.hpp>
#include <helios_private/hout_creation/definition_generation/to_string_methods.hpp>
#include <helios_private/hout_creation/definition_generation/tuple_constructor.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/hout_stmt_compilation.hpp>
#include <helios_private/pst_layer/stmts_from_aggregate.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/pst_symbol_data.hpp>
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
				const auto unlocked_body = body.unlock(ctx);

				if (unlocked_body->getType() == pst::CodeBlockOrStmt::Type::SingleStmt) {
					// The `fun abc() = expr;` case.
					auto function_body
						= queryCodeOfSingleStmtFunctionBody(ctx, unlocked_body, decl.return_type);

					return std::make_shared<const code::CodeBlock>(std::move(function_body));
				}

				CORE_ASSERT(
					unlocked_body->getType() == pst::CodeBlockOrStmt::Type::CodeBlock,
					"This should not happen"
				);

				auto output_body = compileCodeOfCodeBlock(ctx, body, decl.return_type);

				const bool ends_with_return
					= !output_body->statements.empty()
				   && dynamic_cast<const code::ReturnStmt*>(output_body->statements.back().get())
				          != nullptr;

				// Only block-bodied global main functions receive an implicit return.
				if (!isGlobalMain(original_symbol) || ends_with_return) return output_body;

				auto mutable_body = output_body->clone();
				auto zero = numeric_value::NumericValue::createOfType(decl.return_type.getType(), 0)
				                .expect("Failed to create implicit main return value.");

				const auto origin    = code::pstOrigin(unlocked_body).generatedFrom();
				auto       zero_expr = makeBox<code::LiteralNumericExpr>(ctx, origin, zero);

				mutable_body->statements.emplace_back(
					makeBox<code::ReturnStmt>(origin, std::move(zero_expr))
				);

				return std::make_shared<const code::CodeBlock>(std::move(*mutable_body));
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

			void visitConstructor(pst::Access<pst::Constructor>) final {
				ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
					"Support for user defined constructors will be deleted."
				));
				query::throwFailed();
			}

			void visitCopyConstructor(pst::Access<pst::CopyConstructor> stmt) final {
				// declaration:
				auto& decl = ctx.query<QueryDeclOfFun>(original_symbol)->valueOrThrow();
				validateConstructorSource(stmt, decl);

				// body:
				auto output_body = processBody(decl, stmt->getBody());
				this->out.emplace(HOUTFunction(code::pstOrigin(stmt), &decl, output_body));
			}

			void visitDestructor(pst::Access<pst::Destructor> stmt) final {
				// declaration:
				auto& decl = ctx.query<QueryDeclOfFun>(original_symbol)->valueOrThrow();

				// body: a destructor body is always a code block
				auto output_body = compileCodeOfCodeBlock(ctx, stmt->getBody(), decl.return_type);
				this->out.emplace(HOUTFunction(code::pstOrigin(stmt), &decl, output_body));
			}

			// Validates a user-defined copy constructor's source parameter. The copy
			// constructor must declare exactly one parameter, which must be a constant reference
			// to its own class.
			template<class ConstructorElement>
			void validateConstructorSource(
				pst::Access<ConstructorElement> stmt, const HOUTFunctionDeclaration& decl
			) {
				const auto class_type = classMemberOwner(original_symbol);

				const auto params_source = stmt->getParams().unlock(ctx)->getStablePosition();

				if (decl.parameters.size() != 1) {
					ctx.logInt(makeBox<dia::PlaceholderError>(
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
				// @TODO: #2104 Require the reference to be `const` once `const ref T` actually
				// resolves to an immutable reference.
				if (!is_reference || !is_matching_class) {
					ctx.logInt(makeBox<dia::PlaceholderError>(
						base::strConcat(
							"A copy constructor's parameter must be a reference to its own "
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

			// Generated symbol data.
			variant_match(sym_ref->other) {
				variant_case_novalue(PstImplementedSemantics, ClassMemberSemantics) {
					CORE_ASSERT(
						isFunctionLike(kind(key)),
						"Function creation called on non-function, non-method and non-constructor "
						"symbol"
					);
					HOUTFunctionMaker func_maker(ctx, key);
					stmt(ctx, key).value()->acceptVisitor(func_maker);

					// A visitor that could not build the function has reported why.
					if_opt_none(func_maker.out) return query::Failed();

					return func_maker.out.value();
				}
				variant_case(BuiltinSemantics, data) {
					return getBuiltinImpl(ctx, key, data.builtin);
				}
				variant_case(defgen::Constructor, ctor) {
					switch (ctor.kind) {
					case defgen::Constructor::Kind::Implicit:
						switch (ctor.type.getKind()) {
						case tsh::Kind::Class:
							return ctx
							    .query<defgen::QueryImplicitClassConstructor>(
									ctor.type.as<tsh::ClassAbstractType>()
								)
							    ->valueOrThrow();
						case tsh::Kind::Tuple:
							return ctx
							    .query<defgen::QueryTuplePackConstructor>(
									ctor.type.as<tsh::TupleAbstractType>()
								)
							    ->valueOrThrow();
						default:
							CORE_PANIC("Unhandled implicit constructor type.");
						}
					case defgen::Constructor::Kind::Default:
						switch (ctor.type.getKind()) {
						case tsh::Kind::Class:
							return ctx
							    .query<defgen::QueryDefaultClassConstructor>(
									ctor.type.as<tsh::ClassAbstractType>()
								)
							    ->valueOrThrow();
						case tsh::Kind::StaticArray:
							return ctx
							    .query<defgen::QueryDefaultStaticArrayConstructor>(
									ctor.type.as<tsh::StaticArrayAbstractType>()
								)
							    ->valueOrThrow();
						case tsh::Kind::Tuple:
							return ctx
							    .query<defgen::QueryDefaultTupleConstructor>(
									ctor.type.as<tsh::TupleAbstractType>()
								)
							    ->valueOrThrow();
						default:
							CORE_PANIC("Unhandled default constructor type.");
						}
					case defgen::Constructor::Kind::Copy:
						return ctx.query<defgen::QueryDefaultCopyConstructor>(ctor.type)
						    ->valueOrThrow();
					}
					CORE_UNREACHABLE();
				}
				variant_case(defgen::Method, method) {
					switch (method.kind) {
					case defgen::Method::Kind::DefaultDestructor:
						return ctx.query<defgen::QueryDefaultDestructor>(method.owner_type)
						    ->valueOrThrow();
					case defgen::Method::Kind::ToString:
						return ctx.query<defgen::QueryToStringMethod>(method.owner_type)
						    ->valueOrThrow();
					case defgen::Method::Kind::LengthMethod:
						return ctx.query<defgen::QueryLengthMethod>(method.owner_type)
						    ->valueOrThrow();
					}
					CORE_UNREACHABLE();
				}
				variant_case(defgen::BuiltinTemplatedSymbol, symbol_data) {
					return getBuiltinImpl(ctx, key, symbol_data.getBuiltinKind());
				}
				variant_case(defgen::ReplInputWrapper, input) {
					return repl::getReplInputFunction(ctx, input);
				}
				variant_default {
					CORE_PANIC(base::strConcat(
						"QueryCodeOfFun: symbol '",
						prettyDebugPrint(key, ctx),
						"' is neither a PST nor a supported generated function"
					));
				}
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCodeOfFun);
}

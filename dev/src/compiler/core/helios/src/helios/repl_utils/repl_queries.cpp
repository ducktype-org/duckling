// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "repl_queries.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/hout/elements.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/global_data_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/hout_stmt_compilation.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <logger/logger.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <vm/api/vm.hpp>

namespace compiler::repl {

	helios::HOUTFunction getReplInputFunction(
		query::Context& ctx, helios::SymID sym, const helios::defgen::ReplInputWrapper& input
	) {
		CORE_DEV_LOG(REPL, "getReplInputFunction: Starting\n");

		auto code_block = std::make_shared<helios::code::CodeBlock>();

		variant_match(input.element) {
			variant_case(helios::defgen::ReplInputWrapper::Instruction, instr) {
				CORE_DEV_LOG(REPL, "Compiling instruction into HOUT code block\n");

				auto element = pst::LangElement::getByStableHash(instr.stmt);

				code_block
					= std::make_shared<helios::code::CodeBlock>(helios::compileSingleStatement(
						ctx,
						element.dynamicCast<pst::Stmt>(),
						tsh::SymbolType<>::withDefaults(tsh::getUnitType())
					));

				code_block->statements.emplace_back(
					base::makeBox<helios::code::VoidReturnStmt>(helios::code::generatedOrigin())
				);
			}
			variant_case(helios::defgen::ReplInputWrapper::Expression, expr) {
				CORE_DEV_LOG(REPL, "Creating ReturnStmt for value expression\n");

				// `return <expr>` is the last use of the expression, so a temporary is moved out
				// implicitly. A value that may not be handed over, like a global of a type that is
				// not trivially copyable, is returned as it is.
				namespace coercions = helios::coercions;

				const coercions::Coercion coercion = coercions::coercionOf(
					ctx,
					expr.expr.ref(),
					expr.expr->expression_type.getSymbolType(),
					coercions::HandOverKind::LastUse
				);
				BoxOrCRef<helios::code::Expr> hout_expr = expr.expr.ref();
				if (coercion.isValid())
					hout_expr = coercions::applyCoercionFromRef(ctx, coercion, expr.expr.ref());

				code_block->statements.emplace_back(base::makeBox<helios::code::ReturnStmt>(
					helios::code::generatedOrigin(), std::move(hout_expr)
				));
			}
		}

		auto& decl = ctx.query<helios::QueryDeclOfFun>(sym)->valueOrThrow();

		CORE_DEV_LOG(REPL, "getReplInputFunction completed successfully\n");
		return helios::HOUTFunction{ helios::code::generatedOrigin(), &decl, code_block };
	}

	query::QResult<helios::SymID> queryReplExpressionWrapperSymbol(
		query::Context& ctx, pst::AccessLocked<pst::ExprStmt> expr_stmt, u64 counter
	) {
		auto unlocked    = expr_stmt.unlock(ctx);
		auto expr_holder = unlocked->getExpr().unlock(ctx);

		UNPACK_QRESULT_CREF_TO_BOX(
			auto hout_expr =, ctx.query<helios::QueryHoutOfExpr>(expr_holder->getExpr())
		);
		// The wrappers are told apart by their ReplInputWrapper data, the name only has to be
		// stable, so that the symbol can be looked up again from the data alone.
		return ctx.query<helios::defgen::QueryGeneratedSymbol>({
			.name                  = base::StrID("__repl_input_wrapper"),
			.generated_symbol_data = helios::defgen::ReplInputWrapper(
				helios::defgen::ReplInputWrapper::Expression{ hout_expr->clone() }, counter
			),
		});
	}

	helios::SymID getVariableSymID(query::Context& ctx, pst::AccessLocked<pst::Variable> var_stmt) {
		return ctx.query<helios::QuerySymbolOfSTMT>({ var_stmt }).valueOrThrow();
	}

	helios::SymID queryReplInstructionWrapperSymbol(
		query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt, u64 counter
	) {
		return ctx.query<helios::defgen::QueryGeneratedSymbol>({
			.name                  = base::StrID("__repl_input_wrapper"),
			.generated_symbol_data = helios::defgen::ReplInputWrapper(
				helios::defgen::ReplInputWrapper::Instruction{ stmt.unlock(ctx)->getHash() }, counter
			),
		});
	}

	query::QResult<helios::SymID> queryReplGlobalInitializerWrapperSymbol(
		query::Context& ctx, pst::AccessLocked<pst::Variable> var_stmt, u64 counter
	) {
		UNPACK_QRESULT(auto symbol =, ctx.query<helios::QuerySymbolOfSTMT>({ var_stmt }));
		UNPACK_QRESULT_CREF(
			const auto& global_data =, ctx.query<helios::QueryHOUTGlobalData>(symbol)
		);

		CORE_ASSERT(
			global_data.data_type == helios::HOUTGlobalDataType::Variable,
			"A global initializer wrapper expects a variable declaration"
		);

		auto expr = helios::getGlobalConstructorExpr(ctx, &global_data);

		return ctx.query<helios::defgen::QueryGeneratedSymbol>({
			.name                  = base::StrID("__repl_input_wrapper"),
			.generated_symbol_data = helios::defgen::ReplInputWrapper(
				helios::defgen::ReplInputWrapper::Expression{ std::move(expr) }, counter
			),
		});
	}

	helios::SymID queryReplEmptyVariableSymbol(query::Context& ctx, helios::SymID variable_symbol) {
		return ctx.query<helios::defgen::QueryGeneratedSymbol>({
			.name = helios::name(variable_symbol),
			.generated_symbol_data
			= helios::defgen::ReplEmptyVariable{ .original_variable = variable_symbol },
		});
	}

	helios::SymID queryCompTimeExpressionWrapperSymbol(
		query::Context& ctx, base::StrID sym_name, Box<helios::code::Expr> expr
	) {
		return ctx.query<helios::defgen::QueryGeneratedSymbol>({
			.name                  = sym_name,
			.generated_symbol_data = helios::defgen::ReplInputWrapper(
				helios::defgen::ReplInputWrapper::Expression{ std::move(expr) }
			),
		});
	}
}  // namespace compiler::repl

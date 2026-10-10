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

				BoxOrCRef<helios::code::Expr> hout_expr = expr.expr.ref();
				// The move <expr> is added in the HOUT and in this case it won't be added
				// automatically. And we don't want to run destructor of a result temporary,
				// if it's non-trivially destructible. return <expr>.
				if (expr.expr->expression_type.getValueCategory().isMovableFrom())
					hout_expr = makeBox<helios::code::MoveExpr>(
						ctx,
						expr.expr->origin.generatedFrom(),
						expr.expr->clone(),
						helios::code::MoveExpr::MoveKind::Implicit
					);

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
		query::Context& ctx, pst::AccessLocked<pst::ExprStmt> expr_stmt, base::Optional<u64> counter
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
		query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt, base::Optional<u64> counter
	) {
		return ctx.query<helios::defgen::QueryGeneratedSymbol>({
			.name                  = base::StrID("__repl_input_wrapper"),
			.generated_symbol_data = helios::defgen::ReplInputWrapper(
				helios::defgen::ReplInputWrapper::Instruction{ stmt.unlock(ctx)->getHash() },
				counter
			),
		});
	}

	helios::SymID queryReplGlobalInitializerWrapperSymbol(
		query::Context& ctx, pst::AccessLocked<pst::Variable> var_stmt, base::Optional<u64> counter
	) {
		auto symbol = ctx.query<helios::QuerySymbolOfSTMT>({ var_stmt }).valueOrThrow();

		const auto& global_data = ctx.query<helios::QueryHOUTGlobalData>(symbol)->valueOrThrow();

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

	helios::SymID queryHoutExpressionWrapperSymbol(
		query::Context& ctx,
		base::StrID sym_name,
		Box<helios::code::Expr> expr,
		base::Optional<u64> counter
	) {
		return ctx.query<helios::defgen::QueryGeneratedSymbol>({
			.name                  = sym_name,
			.generated_symbol_data = helios::defgen::ReplInputWrapper(
				helios::defgen::ReplInputWrapper::Expression{ std::move(expr) }, counter
			),
		});
	}
}  // namespace compiler::repl

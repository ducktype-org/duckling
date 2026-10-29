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
		query::Context& ctx, const helios::defgen::ReplInputWrapper& input
	) {
		CORE_DEV_LOG(REPL, "getReplInputFunction: Starting\n");

		auto element = pst::LangElement::getByStableHash(input.pst_element_hash);

		auto code_block = std::make_shared<helios::code::CodeBlock>();

		// Inputs that boil down to a single expression (a bare expression and a global initializer)
		// share the way the body is built: the expression is returned when the wrapper has a
		// meaningful result, and executed as a plain statement otherwise.
		auto emplace_expression_body
			= [&code_block](BoxOrCRef<helios::code::Expr> expr, bool returns_value) {
				  if (returns_value) {
					  CORE_DEV_LOG(REPL, "Creating ReturnStmt for value expression\n");
					  code_block->statements.emplace_back(base::makeBox<helios::code::ReturnStmt>(
						  helios::code::generatedOrigin(), std::move(expr)
					  ));
				  } else {
					  CORE_DEV_LOG(REPL, "Creating ExprStmt for expression without a result\n");
					  code_block->statements.emplace_back(base::makeBox<helios::code::ExprStmt>(
						  helios::code::generatedOrigin(), std::move(expr)
					  ));
				  }
			  };

		switch (input.type) {
		case helios::defgen::ReplInputWrapper::Type::Expression: {
			auto expr_stmt   = element.dynamicCast<pst::ExprStmt>().unlock(ctx);
			auto expr_holder = expr_stmt->getExpr().unlock(ctx);

			CORE_DEV_LOG(REPL, "Converting expression to HOUT...\n");
			auto hout_expr
				= ctx.query<helios::QueryHoutOfExpr>(expr_holder->getExpr())->valueOrThrow().ref();

			emplace_expression_body(
				hout_expr, hout_expr->expression_type.getSymbolType().toString() != "void"
			);
			break;
		}
		case helios::defgen::ReplInputWrapper::Type::GlobalInitializer: {
			CORE_DEV_LOG(REPL, "Building the initializer of a global variable\n");

			auto var_stmt = element.dynamicCast<pst::Variable>();

			auto symbol = ctx.query<helios::QuerySymbolOfSTMT>({ var_stmt }).valueOrThrow();

			const auto& global_data
				= ctx.query<helios::QueryHOUTGlobalData>(symbol)->valueOrThrow();

			CORE_ASSERT(
				global_data.data_type == helios::HOUTGlobalDataType::Variable,
				"A global initializer wrapper expects a variable declaration"
			);

			emplace_expression_body(helios::getGlobalConstructorExpr(ctx, &global_data), false);

			code_block->statements.emplace_back(
				base::makeBox<helios::code::VoidReturnStmt>(helios::code::generatedOrigin())
			);
			break;
		}
		case helios::defgen::ReplInputWrapper::Type::Instruction: {
			CORE_DEV_LOG(REPL, "Compiling instruction into HOUT code block\n");

			code_block = std::make_shared<helios::code::CodeBlock>(helios::compileSingleStatement(
				ctx,
				element.dynamicCast<pst::Stmt>(),
				tsh::SymbolType<>::withDefaults(tsh::getUnitType())
			));

			code_block->statements.emplace_back(
				base::makeBox<helios::code::VoidReturnStmt>(helios::code::generatedOrigin())
			);
			break;
		}
		}

		CORE_DEV_LOG(REPL, "Creating function declaration\n");
		auto  symbol = ctx.query<helios::defgen::QueryGeneratedSymbol>({
			 .name                  = base::StrID("__repl_input_wrapper__"),
			 .generated_symbol_data = input,
        });
		auto& decl   = ctx.query<helios::QueryDeclOfFun>(symbol)->valueOrThrow();

		CORE_DEV_LOG(REPL, "getReplInputFunction completed successfully\n");
		return helios::HOUTFunction{ helios::code::generatedOrigin(), &decl, code_block };
	}

	query::QResult<helios::SymID> queryReplExpressionWrapperSymbol(
		query::Context& ctx, pst::AccessLocked<pst::ExprStmt> expr_stmt, u64 counter
	) {
		auto unlocked    = expr_stmt.unlock(ctx);
		auto expr_holder = unlocked->getExpr().unlock(ctx);

		auto hout_expr_result = ctx.query<helios::QueryHoutOfExpr>(expr_holder->getExpr());
		if (hout_expr_result->hasFailed()) return query::Failed();

		auto return_type = hout_expr_result->valueOrPanic().ref()->expression_type.getSymbolType();
		CORE_DEV_LOG(REPL, "Expression return type: ", return_type.toString(), "\n");

		// The wrappers are told apart by their ReplInputWrapper data, the name only has to be
		// stable, so that the symbol can be looked up again from the data alone.
		return ctx.query<helios::defgen::QueryGeneratedSymbol>({
			.name                  = base::StrID("__repl_input_wrapper__"),
			.generated_symbol_data = helios::defgen::ReplInputWrapper(
				helios::defgen::ReplInputWrapper::Type::Expression,
				counter,
				return_type,
				unlocked->getHash()
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
			.name                  = base::StrID("__repl_input_wrapper__"),
			.generated_symbol_data = helios::defgen::ReplInputWrapper(
				helios::defgen::ReplInputWrapper::Type::Instruction,
				counter,
				tsh::SymbolType<>::withDefaults(tsh::getUnitType()),
				stmt.unlock(ctx)->getHash()
			),
		});
	}

	helios::SymID queryReplGlobalInitializerWrapperSymbol(
		query::Context& ctx, pst::AccessLocked<pst::Variable> var_stmt, u64 counter
	) {
		return ctx.query<helios::defgen::QueryGeneratedSymbol>({
			.name                  = base::StrID("__repl_input_wrapper__"),
			.generated_symbol_data = helios::defgen::ReplInputWrapper(
				helios::defgen::ReplInputWrapper::Type::GlobalInitializer,
				counter,
				tsh::SymbolType<>::withDefaults(tsh::getUnitType()),
				var_stmt.unlock(ctx)->getHash()
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
}  // namespace compiler::repl

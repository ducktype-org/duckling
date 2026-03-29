#include "repl_queries.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <helios/hout/elements.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/hout_stmt_compilation.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <logger/logger.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <vm/api/vm.hpp>

namespace compiler::repl {

	base::Bit256 QueryReplExpressionWrapper_Key::queryUnstablePerfectHash() const {
		auto expr_hash = expr_stmt.illegalAccess().value()->getHash();

		return hashing::justHash<hashing::SHA256>(expr_hash, counter);
	}

	struct IMPLEMENT_QUERY(QueryReplExpressionWrapper, helios::HOUTFunction) {
		static auto provide(query::Context& ctx, QKey key) -> PResult {
			auto expr_stmt = key.expr_stmt.unlock(ctx);

			auto expr_holder = expr_stmt->getExpr().unlock(ctx);

			CORE_DEV_LOG(REPL, "Converting expression to HOUT...\n");
			auto hout_expr_result = ctx.query<helios::QueryHoutOfExpr>(expr_holder->getExpr());

			CORE_ASSERT(
				hout_expr_result->hasValue(),
				"Failed to convert expression to HOUT in REPL expression wrapper"
			);


			auto hout_expr   = hout_expr_result->valueOrPanic()->clone();
			auto return_type = hout_expr->expression_type.getSymbolType();

			CORE_DEV_LOG(REPL, "Expression return type: ", return_type.toString(), "\n");

			auto code_block = std::make_shared<helios::code::CodeBlock>();

			// @TODO: #1817 Instead of returning the value, we should call a generic
			// print() function here that works for any type. This would eliminate the need
			// to return values and manually convert them based on type in repl_dvm_helpers.cpp
			if (return_type.toString() == "void") {
				CORE_DEV_LOG(REPL, "Creating ExprStmt for void expression\n");
				auto void_expr_stmt = base::makeBox<helios::code::ExprStmt>(
					helios::code::generatedOrigin(), std::move(hout_expr)
				);
				code_block->statements.emplace_back(std::move(void_expr_stmt));
			} else {
				CORE_DEV_LOG(REPL, "Creating ReturnStmt for value expression\n");
				auto return_stmt = base::makeBox<helios::code::ReturnStmt>(
					helios::code::generatedOrigin(), std::move(hout_expr)
				);
				code_block->statements.emplace_back(std::move(return_stmt));
			}

			// This below is just to create a unique symbol for the REPL expression wrapper
			CORE_DEV_LOG(REPL, "Creating synthetic symbol for wrapper function\n");
			auto synthetic_symbol = ctx.query<helios::defgen::QueryGeneratedSymbol>(
				{ .name = base::StrID("__repl_expr_wrapper__"),
			      .generated_symbol_data
			      = helios::defgen::GeneratedSymbolData{ helios::defgen::GeneratedSymbolData::ReplExpressionWrapper{
					  .counter = key.counter, .return_type = return_type } } }
			);

			CORE_DEV_LOG(REPL, "Creating function declaration\n");
			auto& decl = ctx.query<helios::QueryDeclOfFun>(synthetic_symbol)->valueOrThrow();

			CORE_DEV_LOG(REPL, "QueryReplExpressionWrapper completed successfully\n");
			return { helios::code::generatedOrigin(), &decl, code_block };
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryReplExpressionWrapper)

	base::Bit256 QueryReplInstructionWrapper_Key::queryUnstablePerfectHash() const {
		auto stmt_hash = stmt.illegalAccess().value()->getHash();
		return hashing::justHash<hashing::SHA256>(stmt_hash, counter);
	}

	struct IMPLEMENT_QUERY(QueryReplInstructionWrapper, helios::HOUTFunction) {
		static auto provide(query::Context& ctx, QKey key) -> PResult {
			CORE_DEV_LOG(REPL, "QueryReplInstructionWrapper: Starting\n");

			// Unit (not Void) is the correct return type for procedures.
			// Per the language spec: "void ... cannot be the type of a variable, or cannot
			// be returned from a function".
			// Unit is "the return type of a procedure, i.e. a function without a
			// meaningful result" and is properly lowered to ReturnVoid by MIR/LIR.
			auto void_type = tsh::SymbolType<>{
				tsh::getUnitType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};

			CORE_DEV_LOG(REPL, "Compiling instruction into HOUT code block\n");
			auto code_block = std::make_shared<helios::code::CodeBlock>(
				helios::compileSingleStatement(ctx, key.stmt, void_type)
			);

			// Void functions require an explicit return statement at the end.
			code_block->statements.emplace_back(
				base::makeBox<helios::code::VoidReturnStmt>(helios::code::generatedOrigin())
			);

			CORE_DEV_LOG(REPL, "Creating synthetic symbol for instruction wrapper\n");
			auto synthetic_symbol = ctx.query<helios::defgen::QueryGeneratedSymbol>(
				{ .name = base::StrID("__repl_instr_wrapper__"),
			      .generated_symbol_data
			      = helios::defgen::GeneratedSymbolData{ helios::defgen::GeneratedSymbolData::ReplInstructionWrapper{
					  .counter = key.counter } } }
			);

			CORE_DEV_LOG(REPL, "Creating function declaration\n");
			auto& decl = ctx.query<helios::QueryDeclOfFun>(synthetic_symbol)->valueOrThrow();

			CORE_DEV_LOG(REPL, "QueryReplInstructionWrapper completed successfully\n");
			return { helios::code::generatedOrigin(), &decl, code_block };
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryReplInstructionWrapper)
}  // namespace compiler::repl

#include "repl_queries.hpp"

#include "helios/hout/origin.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/attribute.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/dotted_name.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <helios/hout/elements.hpp>
#include <helios/queries.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
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
			auto synthetic_symbol = ctx.query<helios::houtgen::QueryGeneratedSymbol>(
				{ .name = base::StrID("__repl_expr_wrapper__"),
			      .generated_symbol_data
			      = helios::houtgen::GeneratedSymbolData{ helios::houtgen::GeneratedSymbolData::ReplExpressionWrapper{
					  .counter = key.counter, .return_type = return_type } } }
			);

			CORE_DEV_LOG(REPL, "Creating function declaration\n");
			auto decl_ptr = new helios::HOUTFunctionDeclaration(
				synthetic_symbol,
				return_type,
				std::vector<helios::code::Parameter>{},
				helios::code::generatedOrigin()
			);
			auto decl = base::CRef<helios::HOUTFunctionDeclaration>(decl_ptr);

			CORE_DEV_LOG(REPL, "QueryReplExpressionWrapper completed successfully\n");
			return { helios::code::generatedOrigin(), decl, code_block };
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryReplExpressionWrapper)
}  // namespace compiler::repl

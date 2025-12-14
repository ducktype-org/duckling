#include "repl_queries.hpp"

#include <driver_private/operations.hpp>
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
#include <helios/symbols/simple.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_code_generation/class_constructors.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <query_framework/query_impl.hpp>

#include <vm/api/vm.hpp>

namespace compiler::repl {

	base::Bit256 QueryReplExpressionWrapper_Key::queryUnstablePerfectHash() const {
		return { counter };
	}

	struct IMPLEMENT_QUERY(QueryReplExpressionWrapper, helios::HOUTFunction) {
		static auto provide(query::Context& ctx, QKey key) -> PResult {
			auto expr_stmt = key.expr_stmt.unlock(ctx);

			auto expr_holder      = expr_stmt->getExpr().unlock(ctx);
			auto hout_expr_result = ctx.query<helios::QueryHoutOfExpr>(expr_holder->getExpr());

			auto hout_expr
				= std::move(hout_expr_result).expect("Failed to convert expression to HOUT");

			auto return_type = hout_expr->expression_type.getSymbolType();

			auto return_stmt = base::makeBox<helios::code::ReturnStmt>(std::move(hout_expr));

			auto code_block = std::make_shared<helios::code::CodeBlock>();
			code_block->statements.emplace_back(std::move(return_stmt));

			// This below is just to create a unique symbol for the REPL expression wrapper
			auto synthetic_symbol = ctx.query<helios::houtgen::QueryGeneratedSymbol>(
				{ .name = base::StrID("__repl_expr_wrapper__"),
			      .generated_symbol_data
			      = helios::houtgen::GeneratedSymbolData{ helios::houtgen::GeneratedSymbolData::ReplExpressionWrapper{
					  .counter = key.counter, .return_type = return_type } } }
			);
			auto decl_ptr = new helios::HOUTFunctionDeclaration(
				synthetic_symbol, return_type, std::vector<helios::code::Parameter>{}
			);
			auto decl = base::CRef<helios::HOUTFunctionDeclaration>(decl_ptr);

			return { decl, code_block };
		}

		// When counter is used as part of the key, caching is not very useful.
		// TODO: decide if we want to cache expressions based on their content instead.
		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryReplExpressionWrapper)

	// Right now it's exactly the same as QueryModuleHOUT from helios,
	// but in the future it might differ (e.g., include some REPL-specific functions).
	struct IMPLEMENT_QUERY(QueryReplModuleHOUT, helios::HOUTUnit) {
		static auto provide(query::Context& ctx, QKey key) -> PResult {
			auto scopes = ctx.query<helios::QueryScopesInModule>(key);

			helios::HOUTUnit out;
			for (auto scope: *scopes) {
				auto symbols_in_scope = ctx.query<helios::QuerySymbolsInScope>(scope);

				for (auto sym: *symbols_in_scope) {
					// grab constants:
					if (helios::kind(sym) == helios::SymbolKind::Const)
						out.glob_data.emplace_back(sym, ctx, helios::HOUTGlobalDataType::Constant);
					if (helios::kind(sym) == helios::SymbolKind::Variable
					    and helios::isGlobalVar(ctx, sym))
						out.glob_data.emplace_back(sym, ctx, helios::HOUTGlobalDataType::Variable);
					// grab functions:
					if (helios::kind(sym) == helios::SymbolKind::Function)
						out.functions.push_back(ctx.query<helios::QueryCodeOfFun>(sym));
					if (helios::kind(sym) == helios::SymbolKind::Class)
						appendClassConstructors(out.functions, sym, ctx);
				}
			}

			return out;
		}

		/**
		 * Append the constructors of a class to the provided vector of functions.
		 * @param out_functions The vector of functions to be modified.
		 * @param class_sym The symbol of the class, whose constructors are to be appended.
		 * @param ctx The query context.
		 */
		static void appendClassConstructors(
			std::vector<helios::HOUTFunction>& out_functions,
			const helios::SymID                class_sym,
			query::Context&                    ctx
		) {
			CORE_ASSERT(
				helios::kind(class_sym) == helios::SymbolKind::Class,
				"Invalid argument exception: expected class symbol"
			);

			// For now, we handle only the class's primary constructor.
			// @TODO: #1290 Handle auxiliary constructors.

			const auto class_type
				= ctx.query<helios::QueryTypeFromDefinition>(class_sym)
			          ->expect("Not handling errors here yet... (generating class constructor)")
			          .getType()
			          .as<tsh::ClassAbstractType>();
			const auto implicit_ctor
				= ctx.query<helios::houtgen::QueryImplicitClassConstructor>(class_type);
			out_functions.push_back(*implicit_ctor);
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryReplModuleHOUT)

}  // namespace compiler::repl

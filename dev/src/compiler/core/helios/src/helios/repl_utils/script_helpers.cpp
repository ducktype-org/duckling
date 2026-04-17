#include "script_helpers.hpp"

#include <frontend/module_tree/functors.hpp>
#include <helios/hout/elements.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <logger/logger.hpp>

namespace compiler::repl {
	/**
	 * @brief Builds a synthetic script `main` HOUT function.
	 * @note This builder intentionally remains a separate type so `helios::HOUTFunction`
	 * can grant narrow friend access for its private constructor in `hout.hpp`.
	 */
	struct ScriptMainWrapperBuilder final {
		static auto build(
			query::Context&                   ctx,
			base::StrID                       script_id,
			helios::ScopeID                   main_scope,
			const std::vector<helios::SymID>& wrapper_symbols
		) -> helios::HOUTFunction {
			CORE_DEV_LOG(REPL, "Creating synthetic symbol for script main wrapper\n");

			auto synthetic_symbol = ctx.query<helios::defgen::QueryGeneratedSymbol>({
				.name = base::StrID("main"),
				.generated_symbol_data
				= helios::defgen::GeneratedSymbolData{ helios::defgen::GeneratedSymbolData::ScriptMainWrapper{
					.script_id = script_id,
					.scope     = main_scope,
				} },
			});

			CORE_DEV_LOG(REPL, "Creating function declaration for script main\n");
			auto& decl = ctx.query<helios::QueryDeclOfFun>(synthetic_symbol)->valueOrThrow();

			auto code_block = std::make_shared<helios::code::CodeBlock>();
			// Script main only sequences wrapper calls; results are intentionally discarded.
			// Side effects (prints, mutations) are preserved because wrappers are executed.
			for (const auto& wrapper_symbol: wrapper_symbols) {
				auto callee = base::makeBox<helios::code::IdentifierExpr>(
					ctx, helios::code::generatedOrigin(), wrapper_symbol
				);
				std::vector<base::Box<helios::code::Expr>> args;
				auto call_expr = base::makeBox<helios::code::CallExpr>(
					ctx, helios::code::generatedOrigin(), std::move(callee), std::move(args)
				);
				code_block->statements.emplace_back(base::makeBox<helios::code::ExprStmt>(
					helios::code::generatedOrigin(), std::move(call_expr)
				));
			}

			// Return i64 zero to satisfy the VM/LLVM main contract used by the toolchain.
			// This is exactly same return type as in symbol_data.cpp for the ScriptMainWrapper case.
			const auto return_type = tsh::SymbolType<>{
				tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
			auto zero_value = numeric_value::NumericValue::createOfType(return_type.getType(), 0);
			CORE_ASSERT(zero_value.has_value(), "Failed to create script main return literal");
			auto return_expr = base::makeBox<helios::code::LiteralNumericExpr>(
				ctx, helios::code::generatedOrigin(), zero_value.value()
			);
			code_block->statements.emplace_back(base::makeBox<helios::code::ReturnStmt>(
				helios::code::generatedOrigin(), std::move(return_expr)
			));

			CORE_DEV_LOG(REPL, "Script main wrapper built successfully\n");
			return { helios::code::generatedOrigin(), &decl, code_block };
		}
	};

	helios::ScopeID queryScriptMainRootScope(
		query::Context& ctx, frontend::ModuleID terminal_module_id
	) {
		CORE_ASSERT(
			frontend::getModuleRef(terminal_module_id)->isReplModule(),
			"queryScriptMainRootScope expects a REPL/script module"
		);
		return helios::queryRootScopeOfMainModuleFile(ctx, terminal_module_id);
	}

	helios::HOUTFunction buildScriptMainWrapper(
		query::Context&                   ctx,
		base::StrID                       script_id,
		helios::ScopeID                   main_scope,
		const std::vector<helios::SymID>& wrapper_symbols
	) {
		return ScriptMainWrapperBuilder::build(ctx, script_id, main_scope, wrapper_symbols);
	}

}  // namespace compiler::repl

#include "script_helpers.hpp"

#include <frontend/module_tree/functors.hpp>
#include <helios/hout/elements.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <logger/logger.hpp>

namespace compiler::repl {
	/**
	 * @brief Builds a synthetic script `main` HOUT function.
	 * @note This builder intentionally remains a separate type so `helios::HOUTFunction`
	 * can grant narrow friend access for its private constructor in `hout.hpp`.
	 */
	struct ScriptMainWrapperBuilder final {
		static auto build(
			query::Context&                      ctx,
			base::StrID                          script_id,
			helios::ScopeID                      main_scope,
			const std::vector<ScriptMainAction>& actions
		) -> helios::HOUTFunction {
			CORE_DEV_LOG(REPL, "Creating synthetic symbol for script main wrapper\n");

			auto synthetic_symbol = ctx.query<helios::defgen::QueryGeneratedSymbol>({
				.name = base::StrID("main"),
				.generated_symbol_data
				= helios::defgen::ScriptMainWrapper{
					.script_id = script_id,
					.scope     = main_scope,
				},
			});

			CORE_DEV_LOG(REPL, "Creating function declaration for script main\n");
			auto& decl = ctx.query<helios::QueryDeclOfFun>(synthetic_symbol)->valueOrThrow();

			auto code_block = std::make_shared<helios::code::CodeBlock>();
			// Sequence wrapper calls and global-variable initializations in source order.
			for (const auto& action: actions) {
				variant_match(action) {
					variant_case(ScriptMainWrapperCall, call) {
						auto callee = base::makeBox<helios::code::IdentifierExpr>(
							ctx, helios::code::generatedOrigin(), call.wrapper_symbol
						);
						std::vector<base::Box<helios::code::Expr>> args;
						auto call_expr = base::makeBox<helios::code::CallExpr>(
							ctx, helios::code::generatedOrigin(), std::move(callee), std::move(args)
						);
						code_block->statements.emplace_back(base::makeBox<helios::code::ExprStmt>(
							helios::code::generatedOrigin(), std::move(call_expr)
						));
					}
					variant_case(ScriptMainGlobalInit, global_init) {
						// Initialize the global by assigning its initial value, mirroring the
						// normal global constructor lowering (LowerGlobalDataToMIRCtor in
						// mir_queries.cpp). Keep in sync if global init/ctor emission changes.
						auto location = base::makeBox<helios::code::IdentifierExpr>(
							ctx, helios::code::generatedOrigin(), global_init.global_symbol
						);
						code_block->statements.emplace_back(
							base::makeBox<helios::code::AssignmentStmt>(
								helios::code::generatedOrigin(),
								std::move(location),
								global_init.value
							)
						);
					}
				}
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
		query::Context&                      ctx,
		base::StrID                          script_id,
		helios::ScopeID                      main_scope,
		const std::vector<ScriptMainAction>& actions
	) {
		return ScriptMainWrapperBuilder::build(ctx, script_id, main_scope, actions);
	}

	NeutralizedScriptModule neutralizeScriptGlobalInits(
		query::Context& ctx, const helios::HOUTUnit& module_hout
	) {
		NeutralizedScriptModule result;
		// Copying the unit only duplicates the CRef vectors, not the underlying cached data.
		result.unit = module_hout;

		for (auto& global_ref: result.unit.glob_data) {
			const auto& global = *global_ref;

			if (global.data_type != helios::HOUTGlobalDataType::Variable) continue;
			if (!std::holds_alternative<helios::HOUTGlobalVariable>(global.value)) continue;
			// Only mutable globals can be deferred: deferral default-initializes the global
			// eagerly and reassigns the real value from `main`, which immutable globals reject.
			if (global.type.getMutability() != tsh::Mutability::Mutable) continue;

			const auto& real_initializer = std::get<helios::HOUTGlobalVariable>(global.value);

			auto default_init = helios::defgen::getDefaultInitializerExpr(
									ctx, global.type, global.origin.getStablePosition().value()
			)
			                        .valueOrThrow();

			result.deferred_inits.emplace_back(ScriptMainGlobalInit{
				.global_symbol = global.helios_symbol,
				.value         = real_initializer.initial_value.ref(),
			});

			// HOUTGlobalVariable is move-only, so rebuild the entry rather than copying *global_ref.
			result.default_init_storage.push_back(helios::HOUTGlobalData{
				.helios_symbol = global.helios_symbol,
				.origin        = global.origin,
				.original_name = global.original_name,
				.data_type     = global.data_type,
				.value         = helios::HOUTGlobalVariable{ default_init },
				.type          = global.type,
			});
			global_ref = CRef(&result.default_init_storage.back());
		}

		return result;
	}

}  // namespace compiler::repl

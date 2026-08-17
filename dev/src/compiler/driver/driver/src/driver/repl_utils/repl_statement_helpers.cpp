#include "repl_statement_helpers.hpp"

#include "helios/hout/hout.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <frontend/pst_parser/utility.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/queries.hpp>
#include <helios/repl_utils/repl_queries.hpp>

#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>

#include <expected>

namespace compiler::repl {

	base::Ref<frontend::ModuleTree> createEphemeralChainedStatementModule(
		std::string_view                          input,
		const base::Optional<frontend::ModuleID>& parent_module_id,
		u64                                       line_counter,
		std::string_view                          module_name_prefix
	) {
		auto builder = frontend::ModuleTreeBuilder::create();
		builder->setPackageID(base::StrID("repl_session"));
		builder->setMainSourceFile(fs::FileManager::createRandomVirtualFile(input));

		auto module_name = base::strConcat(module_name_prefix, std::to_string(line_counter));
		builder->setName(base::StrID(module_name));

		frontend::ReplData repl_data;
		if (parent_module_id.has_value()) repl_data.m_repl_module_parent = parent_module_id.value();
		builder->setReplModule(repl_data);

		return builder->finalize();
	}

	std::string getStatementModuleName(
		frontend::ModuleID module_id, std::string_view module_name_prefix
	) {
		return base::strConcat(
			module_name_prefix,
			frontend::ModuleTree::getPathComponentHash(module_id).hash.toStringHex()
		);
	}

	std::expected<helios::HOUTUnit, std::string> makeExecutableHOUTUnit(
		query::Context& ctx, const helios::HOUTFunction& wrapper_function
	) {
		helios::HOUTUnit hout_unit;
		hout_unit.functions.emplace_back(&wrapper_function);
		auto result = helios::collectReplicatedSymbols(ctx, hout_unit);
		if (result.isBad()) return std::unexpected("Failed");
		return hout_unit;
	}

	std::expected<base::CRef<helios::HOUTUnit>, std::string> getDefinitionHOUTUnit(
		query::Context& ctx, frontend::ModuleID module_id
	) {
		const auto& hout_qresult = ctx.query<helios::QueryModuleHOUT>(module_id);
		if (hout_qresult->hasFailed()) return std::unexpected("Failed to compile a module.");
		return &hout_qresult->valueOrPanic();
	}

	std::expected<SingleStatementInfo, std::string> classifySingleStatement(
		query::Context& ctx, frontend::ModuleID module_id
	) {
		auto main_file = ctx.query<frontend::QueryMainSourceFile>(module_id);
		auto pst       = getFilePST(ctx, main_file);
		auto root      = pst->getRootElement();

		auto expr_stmt_opt = pst::extractSingleExpression(ctx, root);
		if (expr_stmt_opt.has_value()) {
			auto expr_stmt   = expr_stmt_opt.value();
			auto expr_holder = expr_stmt.unlock(ctx)->getExpr().unlock(ctx);
			auto inner_expr  = expr_holder->getExpr().unlock(ctx);

			// Assignment is syntactically an expression, but its HOUT lowering currently lives in
			// statement compilation. Route it as instruction to use that lowering path.
			if (inner_expr.dynamicCast<pst::expr::Assignment>().has_value())
				return InstructionSingleStatementInfo{ .instruction_stmt = expr_stmt };

			return ExpressionSingleStatementInfo{ .expr_stmt = expr_stmt };
		}

		auto instr_stmt_opt = pst::extractSingleInstruction(ctx, root);
		if (instr_stmt_opt.has_value()) {
			return InstructionSingleStatementInfo{
				.instruction_stmt = instr_stmt_opt.value(),
			};
		}

		auto definition_stmt_opt = pst::extractSingleDefinition(ctx, root);
		if (definition_stmt_opt.has_value())
			return DefinitionSingleStatementInfo{ .definition_stmt = definition_stmt_opt.value() };

		auto single_stmt_opt = pst::extractSingleStatement(ctx, root);
		if (single_stmt_opt.has_value()) {
			auto stmt = single_stmt_opt.value().unlock(ctx);
			return std::unexpected(
				base::strConcat(
					"Unsupported single statement kind for REPL classification: ",
					stmt->elementType(),
					". Expected expression, instruction (if/while/for/block), or "
					"definition/declaration."
				)
			);
		}

		return std::unexpected(
			"Expected exactly one classified statement (expression, instruction, or "
			"definition/declaration)"
		);
	}

	std::expected<StatementWrapperBuildResult, std::string> buildStatementWrapper(
		query::Context& ctx, const SingleStatementInfo& statement_info, u64 counter
	) {
		auto function_result = [&] -> query::QResult<helios::HOUTFunction> {
			variant_match(statement_info) {
				variant_case(repl::ExpressionSingleStatementInfo, val) {
					return ctx.query<QueryReplExpressionWrapper>({
						.expr_stmt = val.expr_stmt,
						.counter   = counter,
					});
				}
				variant_case(repl::InstructionSingleStatementInfo, val) {
					return ctx.query<QueryReplInstructionWrapper>({
						.stmt    = val.instruction_stmt,
						.counter = counter,
					});
				}
				variant_default { CORE_PANIC("Invalid usage."); }
			}
		}();
		if (function_result.hasFailed())
			return std::unexpected(
				"Failed to build REPL expression wrapper (see diagnostics above)."
			);
		auto function = std::move(function_result.valueOrPanic());

		auto mangled_name
			= helios::mangler::getSimpleMangledName(ctx, function.declaration->original_symbol);
		return StatementWrapperBuildResult{
			.wrapper_function  = std::move(function),
			.wrapper_func_name = std::string(mangled_name.strView()),
		};
	}

	std::expected<StatementWrapperBuildResult, std::string> buildStatementWrapper(
		query::Context& ctx, const VariableSingleStatementInfo& statement_info, u64 counter
	) {
	}
}

#include "repl_statement_helpers.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/utility.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/queries.hpp>
#include <helios/repl_utils/repl_queries.hpp>

#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>

namespace compiler::repl {

	base::Ref<frontend::ModuleTree> createChainedStatementModule(
		std::string_view                          input,
		const base::Optional<frontend::ModuleID>& parent_module_id,
		u64                                       line_counter,
		std::string_view                          module_name_prefix
	) {
		auto builder = frontend::ModuleTreeBuilder::create();
		builder->setPackageID(base::generateRandomString(32));
		builder->setMainSourceFile(fs::FileManager::createRandomVirtualFile(input));

		auto module_name = base::strConcat(module_name_prefix, std::to_string(line_counter));
		builder->setName(base::StrID(module_name.c_str()));

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

	helios::HOUTUnit makeExecutableHOUTUnit(const helios::HOUTFunction& wrapper_function) {
		helios::HOUTUnit hout_unit;
		hout_unit.functions.emplace_back(&wrapper_function);
		return hout_unit;
	}

	const helios::HOUTUnit& getDefinitionHOUTUnit(query::Context& ctx, frontend::ModuleID module_id) {
		return ctx.query<helios::QueryModuleHOUT>(module_id)->valueOrThrow();
	}

	std::expected<SingleStatementInfo, std::string> classifySingleStatement(
		query::Context& ctx, frontend::ModuleID module_id
	) {
		auto main_file = ctx.query<frontend::QueryMainSourceFile>(module_id);
		auto pst       = getFilePST(ctx, main_file);
		auto root      = pst->getRootElement();

		auto expr_stmt_opt = pst::extractSingleExpression(ctx, root);
		if (expr_stmt_opt.has_value()) {
			return SingleStatementInfo{
				.kind             = StatementExecutionKind::Expression,
				.expr_stmt        = expr_stmt_opt,
				.instruction_stmt = {},
				.definition_stmt  = {},
			};
		}

		auto instr_stmt_opt = pst::extractSingleInstruction(ctx, root);
		if (instr_stmt_opt.has_value()) {
			return SingleStatementInfo{
				.kind             = StatementExecutionKind::Instruction,
				.expr_stmt        = {},
				.instruction_stmt = instr_stmt_opt,
				.definition_stmt  = {},
			};
		}

		auto top_level_stmt_opt = pst::extractSingleTopLevelStatement(ctx, root);
		if (top_level_stmt_opt.has_value()) {
			return SingleStatementInfo{
				.kind             = StatementExecutionKind::Definition,
				.expr_stmt        = {},
				.instruction_stmt = {},
				.definition_stmt  = top_level_stmt_opt,
			};
		}

		return std::unexpected("Expected exactly one top-level statement");
	}

	std::expected<StatementWrapperBuildResult, std::string> buildStatementWrapper(
		query::Context& ctx, const SingleStatementInfo& statement_info, u64 counter
	) {
		switch (statement_info.kind) {
		case StatementExecutionKind::Expression:
			if (!statement_info.expr_stmt.has_value())
				return std::unexpected("Missing expression statement payload");
			{
				auto wrapper      = ctx.query<QueryReplExpressionWrapper>({
						 .expr_stmt = statement_info.expr_stmt.value(),
						 .counter   = counter,
                });
				auto mangled_name = helios::mangler::getSimpleMangledName(
					ctx, wrapper.declaration->original_symbol
				);
				return StatementWrapperBuildResult{
					.wrapper_function  = std::move(wrapper),
					.wrapper_func_name = std::string(mangled_name.strView()),
				};
			}
		case StatementExecutionKind::Instruction:
			if (!statement_info.instruction_stmt.has_value())
				return std::unexpected("Missing instruction statement payload");
			{
				auto wrapper      = ctx.query<QueryReplInstructionWrapper>({
						 .stmt    = statement_info.instruction_stmt.value(),
						 .counter = counter,
                });
				auto mangled_name = helios::mangler::getSimpleMangledName(
					ctx, wrapper.declaration->original_symbol
				);
				return StatementWrapperBuildResult{
					.wrapper_function  = std::move(wrapper),
					.wrapper_func_name = std::string(mangled_name.strView()),
				};
			}
		case StatementExecutionKind::Definition:
			return std::unexpected("Definitions do not have executable wrappers");
		default:
			return std::unexpected("Unknown statement kind");
		}

		return std::unexpected("Unknown statement kind");
	}

}  // namespace compiler::repl

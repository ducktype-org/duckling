#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <helios/hout/hout.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <query_framework/context/context_fd.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace compiler::repl {
	enum class StatementExecutionKind {
		Expression,
		Instruction,
		Definition,
	};

	struct SingleStatementInfo final {
		StatementExecutionKind                           kind = StatementExecutionKind::Definition;
		base::Optional<pst::AccessLocked<pst::ExprStmt>> expr_stmt;
		base::Optional<pst::AccessLocked<pst::Stmt>>     instruction_stmt;
		base::Optional<pst::AccessLocked<pst::Stmt>>     definition_stmt;
	};

	struct StatementWrapperBuildResult final {
		helios::HOUTFunction wrapper_function;
		std::string          wrapper_func_name;
	};

	/**
	 * @brief Classify a single-statement module into expression/instruction/definition.
	 *
	 * The module is expected to contain exactly one top-level statement.
	 */
	std::expected<SingleStatementInfo, std::string> classifySingleStatement(
		query::Context& ctx, frontend::ModuleID module_id
	);

	/**
	 * @brief Build wrapper function for executable (expression/instruction) statements.
	 *
	 * @param statement_info Must have kind Expression or Instruction.
	 * @param counter Unique wrapper counter used in generated symbol names.
	 */
	std::expected<StatementWrapperBuildResult, std::string> buildStatementWrapper(
		query::Context& ctx, const SingleStatementInfo& statement_info, u64 counter
	);

	/**
	 * @brief Create a REPL/script-style ephemeral module with optional parent linkage.
	 *
	 * This is the shared module-construction primitive used for top-level sequential
	 * statement execution semantics in REPL and script compilation.
	 */
	base::Ref<frontend::ModuleTree> createChainedStatementModule(
		std::string_view                          input,
		const base::Optional<frontend::ModuleID>& parent_module_id,
		u64                                       line_counter,
		std::string_view                          module_name_prefix
	);

	/**
	 * @brief Build the synthetic module name used when compiling/loading a statement module.
	 */
	std::string getStatementModuleName(
		frontend::ModuleID module_id, std::string_view module_name_prefix = "repl_module_"
	);

	/**
	 * @brief Create a temporary HOUT unit that exposes a single executable wrapper function.
	 */
	helios::HOUTUnit makeExecutableHOUTUnit(const helios::HOUTFunction& wrapper_function);

	/**
	 * @brief Retrieve the module HOUT for a definition statement module.
	 */
	const helios::HOUTUnit& getDefinitionHOUTUnit(query::Context& ctx, frontend::ModuleID module_id);

}  // namespace compiler::repl

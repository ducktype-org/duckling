// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/repl_utils/repl_queries.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <query_framework/context/context_fd.hpp>

#include <expected>
#include <string>
#include <string_view>
#include <variant>

namespace compiler::repl {
	/**
	 * @brief Payload for a single expression statement.
	 *
	 * Produced by classifySingleStatement() when a module contains exactly one expression
	 * statement. Consumed by buildStatementWrapper() and REPL/script dispatch sites that execute
	 * the statement through an auto-generated wrapper function.
	 */
	struct ExpressionSingleStatementInfo final {
		pst::AccessLocked<pst::ExprStmt> expr_stmt;
	};

	/**
	 * @brief Payload for a single instruction statement.
	 *
	 * Produced by classifySingleStatement() for executable non-expression statements
	 * (if/while/for/block, etc.). Used by buildStatementWrapper() to create the instruction wrapper
	 * that can be lowered and run.
	 */
	struct InstructionSingleStatementInfo final {
		pst::AccessLocked<pst::Stmt> instruction_stmt;
	};

	/**
	 * @brief Payload for a single top-level definition statement.
	 *
	 * Produced by classifySingleStatement() when input is a definition (function/type/constant
	 * declaration, etc.). Definitions are loaded from module HOUT directly and intentionally do not
	 * go through buildStatementWrapper().
	 */
	struct DefinitionSingleStatementInfo final {
		pst::AccessLocked<pst::Stmt> definition_stmt;
	};

	struct VariableSingleStatementInfo final {
		pst::AccessLocked<pst::Variable> variable_stmt;
	};

	/**
	 * @brief A tagged payload representing exactly one classified REPL/script statement.
	 *
	 * Typical flow:
	 * 1) classifySingleStatement() returns SingleStatementInfo,
	 * 2) executable alternatives are passed to buildStatementWrapper(),
	 * 3) definition alternative is handled via module-level HOUT loading.
	 */
	using SingleStatementInfo = std::variant<
		ExpressionSingleStatementInfo,
		InstructionSingleStatementInfo,
		DefinitionSingleStatementInfo,
		VariableSingleStatementInfo>;

	/**
	 * @brief Result of building an executable wrapper around a single statement (expression or
	 * instruction).
	 */
	struct StatementWrapperBuildResult final {
		helios::HOUTFunction wrapper_function;
		std::string          wrapper_func_name;
	};

	/**
	 * @brief Classify a single-statement module into expression/instruction/definition.
	 *
	 * The module is expected to contain exactly one top-level statement.
	 *
	 * @note Assignment syntax is parsed as ExprStmt, but in REPL/script execution it must flow
	 * through instruction statement compilation (where assignment lowering is implemented). For
	 * this reason, classifySingleStatement() special-cases assignment ExprStmt as instruction.
	 */
	std::expected<SingleStatementInfo, std::string> classifySingleStatement(
		query::Context& ctx, frontend::ModuleID module_id
	);

	/**
	 * @brief Build wrapper function for executable (expression/instruction) statements.
	 *
	 * Returns an error for DefinitionSingleStatementInfo.
	 *
	 * @param statement_info Classified single-statement payload. Should be expression or instruction.
	 * @param counter Unique wrapper counter used in generated symbol names.
	 */
	std::expected<StatementWrapperBuildResult, std::string> buildStatementWrapper(
		query::Context& ctx, const SingleStatementInfo& statement_info, u64 counter
	);

	struct VariableBuildResult final {
		/**
		 * @brief The empty storage of the variable together with the function initializing it.
		 */
		helios::HOUTUnit hout_unit;

		/**
		 * @brief Symbol of the initializing function, to be called in statement order.
		 */
		helios::SymID initializer_function;
	};

	/**
	 * @brief Build the empty storage and the initializing function of a global variable
	 * declaration.
	 *
	 * A top-level `var` is split in two, so that its initial value is constructed in statement
	 * order instead of before the entry point runs together with every other global.
	 *
	 * @param statement_info Classified variable statement.
	 * @param counter Unique wrapper counter used in generated symbol names.
	 */
	std::expected<VariableBuildResult, std::string> buildVariableWrapper(
		query::Context& ctx, const VariableSingleStatementInfo& statement_info, u64 counter
	);

	/**
	 * @brief Create a synthetic REPL/script-style statement module with optional parent linkage.
	 *
	 * This is the shared module-construction primitive used for top-level sequential
	 * statement execution semantics in REPL and script compilation.
	 *
	 * @warning Do NOT call this function from inside query computations.
	 */
	base::Ref<frontend::ModuleTree> createSyntheticChainedStatementModule(
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
	std::expected<helios::HOUTUnit, std::string> makeExecutableHOUTUnit(
		query::Context&                              ctx,
		const helios::HOUTFunction&                  wrapper_function,
		base::Optional<CRef<helios::HOUTGlobalData>> additional_global_var = {}
	);

	/**
	 * @brief Retrieve the module HOUT for a definition statement module.
	 * @return An error when the module failed to compile. The callers are not inside a query that
	 * catches query failures, so the failure has to be reported instead of thrown.
	 */
	std::expected<base::CRef<helios::HOUTUnit>, std::string> getDefinitionHOUTUnit(
		query::Context& ctx, frontend::ModuleID module_id
	);

}  // namespace compiler::repl

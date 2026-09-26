#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <query_framework/context/context.hpp>

#include <functional>
#include <set>
#include <string_view>
#include <vector>

namespace compiler::mir::test_utils {
	CRef<mir::Function> getMIRFunctionByName(frontend::ModuleID module_id, std::string_view name);

	/**
	 * @brief The set of blocks reachable from the entry block by following the terminators.
	 *
	 * `LowerToMIRFunction` already drops unreachable blocks, so a block of a lowered function
	 * that this does not return means the CFG lost an edge somewhere.
	 */
	std::set<BlockID> reachableBlocks(const mir::Function& function);

	/**
	 * @brief Enumerates the control-flow paths from the entry block to the blocks that end the
	 * function.
	 *
	 * A block is never visited twice on the same path, so a loop contributes the paths that go
	 * around it at most once. Used instead of hardcoded block ids, which change whenever the
	 * lowering emits blocks in a different order.
	 */
	std::vector<std::vector<BlockID>> pathsFromEntry(const mir::Function& function);

	/**
	 * @brief The blocks whose instructions (terminator excluded) contain the given operation.
	 */
	std::set<BlockID> blocksWithOperation(const mir::Function& function, Operation operation);

	/**
	 * @brief Counts the blocks terminated by the given operation.
	 */
	u64 countTerminators(const mir::Function& function, Operation operation);

	/**
	 * @brief Positive counterpart of @ref checkForErrorOnCompileModule: lowers a module built
	 * from `module_content` to a MIR unit, asserts that nothing logged an error and hands the
	 * unit to `check`.
	 *
	 * @param module_content The content of the module main source file.
	 * @param check Called with the query context and the lowered unit.
	 */
	void checkLoweredModule(
		std::string_view                                            module_content,
		const std::function<void(query::Context&, const MIRUnit&)>& check
	);

	/**
	 * @brief The MIR function of the given name in an already lowered unit.
	 */
	CRef<mir::Function> functionOfUnit(const mir::MIRUnit& unit, std::string_view name);

	/**
	 * @brief Helper function that check for MIR compilation
	 * errors in a module with given content. Requires that the HELIOS step passes.
	 *
	 * It creates a virtual file from the `module_content` argument
	 * and creates a module tree from it every function call.

	 * @param module_content The content of the module main source file.
	 * @param present_phrases List of phrases that should be present in the logged errors.
	 * @param logged_msg_count Expected number of logged error messages.
	 */
	void checkForErrorOnCompileModule(
		std::string_view                     module_content,
		const std::vector<std::string_view>& present_phrases,
		u64                                  logged_msg_count
	);
}

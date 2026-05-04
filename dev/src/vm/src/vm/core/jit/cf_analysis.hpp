/**
 * @file cf_analysis.hpp
 * @brief Control-flow analysis utilities for splitting lowered bytecode into basic blocks and CFGs.
 */
#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/jit/cf_graph.hpp>
#include <vm/core/safe/low_program/low_program.hpp>

#include <functional>
#include <vector>

namespace vm::jit::cf {
	/**
	 * @brief Finds instruction offsets where basic blocks start.
	 * @param function Lowered function to analyze.
	 * @return Sorted list of basic-block beginnings.
	 */
	std::vector<usize> basicBlockBeginnings(const low::LowFuncData& function);

	/**
	 * @brief Builds a control-flow graph for a lowered function.
	 * @param function Lowered function to analyze.
	 * @return Control-flow graph with computed blocks and edges.
	 */
	inline ControlFlowGraph controlFlowGraph(const low::LowFuncData& function) {
		return { function, basicBlockBeginnings(function) };
	}
}  // namespace vm::jit::cf

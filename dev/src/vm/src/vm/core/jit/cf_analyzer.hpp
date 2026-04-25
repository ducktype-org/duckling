/**
 * @file cf_analyzer.hpp
 * @brief API for splitting lowered bytecode into basic blocks and CFG.
 */
#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/jit/cf_graph.hpp>
#include <vm/core/safe/low_program/low_program.hpp>

#include <functional>
#include <vector>

namespace vm::jit::cf {
	/**
	 * @brief Computes control-flow analysis artifacts from lowered function code.
	 */
	class ControlFlowAnalyzer {
	public:
		ControlFlowAnalyzer() = default;

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
		ControlFlowGraph controlFlowGraph(const low::LowFuncData& function) {
			return { function, basicBlockBeginnings(function) };
		}
	};
}  // namespace vm::jit::cf

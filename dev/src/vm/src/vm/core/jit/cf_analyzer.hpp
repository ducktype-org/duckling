#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/jit/cf_graph.hpp>
#include <vm/core/safe/low_program/low_program.hpp>

#include <functional>
#include <vector>

namespace vm::jit::cf {
	class ControlFlowAnalyzer {
	public:
		ControlFlowAnalyzer() = default;

		std::vector<usize> basicBlockBeginnings(const low::LowFuncData& function);

		ControlFlowGraph controlFlowGraph(const low::LowFuncData& function) {
			return { function, basicBlockBeginnings(function) };
		}
	};
}  // namespace vm::jit::cf

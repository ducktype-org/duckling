#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/safe/low_program/low_program.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <stdexcept>
#include <vector>

namespace vm::jit::cf {
	using BlockID = usize;

	class OutEdges {
	public:
		enum class Kind {
			End,      // No outgoing edges (e.g., return)
			Default,  // Jmp or fallthrough
			JmpIf,
			JmpIfNot,
		};

		OutEdges(): to{ 0, 0 } {}

		[[nodiscard]] usize size() const { return no_edges; }

		[[nodiscard]] Kind kind() const { return op_type; }

		void setCond(Kind kind, BlockID target1, BlockID target2) {
			if (no_edges != 0)
				throw std::runtime_error("Outgoing edges already set for this basic block");
			this->op_type = kind;
			to[0]         = target1;
			to[1]         = target2;
			no_edges      = 2;
		}

		void setDefault(BlockID target) {
			if (no_edges != 0)
				throw std::runtime_error("Outgoing edges already set for this basic block");
			this->op_type = Kind::Default;
			to[0]         = target;
			no_edges      = 1;
		}

		[[nodiscard]] BlockID next() const {
			if (op_type != Kind::Default)
				throw std::runtime_error("This block has no default outgoing edge.");
			return to[0];
		}

		[[nodiscard]] BlockID successTarget() const {
			if (op_type == Kind::JmpIf || op_type == Kind::JmpIfNot) return to[0];
			throw std::runtime_error("This block does not have conditional outgoing edges.");
		}

		[[nodiscard]] BlockID failTarget() const {
			if (op_type == Kind::JmpIf || op_type == Kind::JmpIfNot) return to[1];
			throw std::runtime_error("This block does not have conditional outgoing edges.");
		}

		BlockID operator[](usize index) const { return to.at(index); }

	private:
		std::array<BlockID, 2> to;
		usize                  no_edges{ 0 };
		Kind                   op_type{ Kind::End };
	};

	struct BasicBlock {
	private:
		OutEdges succ;

	public:
		const BlockID id;
		const usize   start;
		const usize   end;

		BasicBlock(BlockID id, usize start, usize end): succ(), id(id), start(start), end(end) {}

		BasicBlock() = delete;

		[[nodiscard]] OutEdges::Kind edgeKind() const { return succ.kind(); }

		[[nodiscard]] BlockID next() const { return succ.next(); }

		[[nodiscard]] BlockID successTarget() const { return succ.successTarget(); }

		[[nodiscard]] BlockID failTarget() const { return succ.failTarget(); }

		void setCondEdge(OutEdges::Kind kind, BlockID target1, BlockID target2) {
			succ.setCond(kind, target1, target2);
		}

		void setDefaultEdge(BlockID target) { succ.setDefault(target); }

		[[nodiscard]] BlockID edge(usize index) const { return succ[index]; }

		[[nodiscard]] usize edgeCount() const { return succ.size(); }
	};

	class ControlFlowGraph {
	private:
		std::vector<BasicBlock> blocks;

		void createCFG(const low::LowFuncData& function, const std::vector<usize>& block_beginnings);

	public:
		ControlFlowGraph() = default;

		ControlFlowGraph(
			const low::LowFuncData& function, const std::vector<usize>& block_beginnings
		) {
			createCFG(function, block_beginnings);
		}

		[[nodiscard]] usize size() const { return blocks.size(); }

		[[nodiscard]] const BasicBlock& getBlock(BlockID id) const {
			CORE_ASSERT(id < blocks.size(), "Invalid block ID");
			return blocks[id];
		}
	};
}  // vm::jit::cf

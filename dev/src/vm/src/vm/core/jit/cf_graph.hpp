/**
 * @file cf_graph.hpp
 * @brief Control-flow graph representation for lowered VM functions.
 */
#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/safe/low_program/low_program.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <vector>

namespace vm::jit::cf {
	/**
	 * @brief Identifier of a basic block in the control-flow graph.
	 */
	using BlockID = usize;

	/**
	 * @brief Compact representation of outgoing edges from a basic block.
	 */
	class OutEdges {
	public:
		/**
		 * @brief Outgoing edge layout produced by the terminating instruction.
		 */
		enum class Kind {
			End,      // No outgoing edges (e.g., return)
			Default,  // Jmp or fallthrough
			JmpIf,
			JmpIfNot,
		};

		OutEdges(): to{ 0, 0 } {}
		OutEdges(const OutEdges&) = default;
		OutEdges& operator=(const OutEdges&) = default;

		/**
		 * @brief Returns the number of outgoing edges.
		 */
		[[nodiscard]] usize size() const { return no_edges; }

		/**
		 * @brief Returns the edge kind.
		 */
		[[nodiscard]] Kind kind() const { return op_type; }

		/**
		 * @brief Configures a conditional edge pair.
		 * @param kind Conditional opcode kind.
		 * @param target1 Target reached when condition succeeds.
		 * @param target2 Target reached when condition fails.
		 */
		void setCond(Kind kind, BlockID target1, BlockID target2) {
			CORE_ASSERT(no_edges == 0, "Outgoing edges already set for this basic block");
			this->op_type = kind;
			to[0]         = target1;
			to[1]         = target2;
			no_edges      = 2;
		}

		/**
		 * @brief Configures a single default edge.
		 * @param target Fallthrough or unconditional jump destination.
		 */
		void setDefault(BlockID target) {
			CORE_ASSERT(no_edges == 0, "Outgoing edges already set for this basic block");
			this->op_type = Kind::Default;
			to[0]         = target;
			no_edges      = 1;
		}

		/**
		 * @brief Returns the default successor.
		 */
		[[nodiscard]] BlockID next() const {
			CORE_ASSERT(op_type == Kind::Default, "This block has no default outgoing edge.");
			return to[0];
		}

		/**
		 * @brief Returns the success successor for conditional branches.
		 */
		[[nodiscard]] BlockID successTarget() const {
			CORE_ASSERT(op_type == Kind::JmpIf || op_type == Kind::JmpIfNot, "This block does not have conditional outgoing edges.");
			return to[0];
		}

		/**
		 * @brief Returns the failure successor for conditional branches.
		 */
		[[nodiscard]] BlockID failTarget() const {
			CORE_ASSERT(op_type == Kind::JmpIf || op_type == Kind::JmpIfNot, "This block does not have conditional outgoing edges.");
			return to[1];
		}

		/**
		 * @brief Returns a mutable reference to the edge target at the given position.
		 * @param index Edge index in [0, size()).
		 */
		BlockID& operator[](usize index) { return to[index]; }

		/**
		 * @brief Returns an immutable reference to the edge target at the given position.
		 * @param index Edge index in [0, size()).
		 */
		const BlockID& operator[](usize index) const { return to[index]; }

	private:
		std::array<BlockID, 2> to;
		usize                  no_edges{ 0 };
		Kind                   op_type{ Kind::End };
	};

	/**
	 * @brief Basic block metadata used by control-flow analyses.
	 */
	struct BasicBlock {
		OutEdges succ;
		const BlockID id;
		const usize   start;
		const usize   end;

		/**
		 * @brief Creates a basic block descriptor.
		 * @param id Numeric block identifier.
		 * @param start Inclusive instruction index where the block starts.
		 * @param end Exclusive instruction index where the block ends.
		 */
		BasicBlock(BlockID id, usize start, usize end): succ(), id(id), start(start), end(end) {}

		BasicBlock() = delete;

		/**
		 * @brief Returns the shape of outgoing edges.
		 */
		[[nodiscard]] OutEdges::Kind edgeKind() const { return succ.kind(); }

		/**
		 * @brief Returns the default successor.
		 */
		[[nodiscard]] BlockID next() const { return succ.next(); }

		/**
		 * @brief Returns the success successor for conditional branches.
		 */
		[[nodiscard]] BlockID successTarget() const { return succ.successTarget(); }

		/**
		 * @brief Returns the failure successor for conditional branches.
		 */
		[[nodiscard]] BlockID failTarget() const { return succ.failTarget(); }

		/**
		 * @brief Sets conditional successors.
		 * @param kind Conditional edge kind.
		 * @param target1 Success destination.
		 * @param target2 Failure destination.
		 */
		void setCondEdge(OutEdges::Kind kind, BlockID target1, BlockID target2) {
			succ.setCond(kind, target1, target2);
		}

		/**
		 * @brief Sets a single default successor.
		 * @param target Default destination.
		 */
		void setDefaultEdge(BlockID target) { succ.setDefault(target); }

		/**
		 * @brief Returns the edge target at index.
		 * @param index Edge index in [0, edgeCount()).
		 */
		[[nodiscard]] BlockID edge(usize index) const { return succ[index]; }

		/**
		 * @brief Returns the number of outgoing edges.
		 */
		[[nodiscard]] usize edgeCount() const { return succ.size(); }
	};

	/**
	 * @brief Control-flow graph built from lowered function bytecode.
	 */
	class ControlFlowGraph {
	private:
		std::vector<BasicBlock> blocks;

		/**
		 * @brief Builds graph blocks and edges from block beginnings.
		 * @param function Lowered function containing bytecode.
		 * @param block_beginnings Sorted block start instruction offsets.
		 */
		void createCFG(const low::LowFuncData& function, const std::vector<usize>& block_beginnings);

	public:
		ControlFlowGraph() = default;

		/**
		 * @brief Creates a control-flow graph from lowered function data.
		 * @param function Lowered function containing bytecode.
		 * @param block_beginnings Sorted block start instruction offsets.
		 */
		ControlFlowGraph(
			const low::LowFuncData& function, const std::vector<usize>& block_beginnings
		) {
			createCFG(function, block_beginnings);
		}

		/**
		 * @brief Returns number of blocks in the graph.
		 */
		[[nodiscard]] usize size() const { return blocks.size(); }

		/**
		 * @brief Returns block metadata by identifier.
		 * @param id Block identifier.
		 */
		[[nodiscard]] const BasicBlock& getBlock(BlockID id) const {
			CORE_ASSERT(id < blocks.size(), "Invalid block ID");
			return blocks[id];
		}

		/**
		 * @brief Builds a CFG containing only selected blocks.
		 * @param block_ids Block ids to keep in the resulting graph.
		 * @return A remapped CFG subgraph with out-of-subset edges redirected.
		 * @note Current implementation redirects external edges to a synthetic dummy block.
		 */
		[[nodiscard]] ControlFlowGraph subgraph(const std::vector<BlockID>& block_ids) const;
	};
}  // vm::jit::cf

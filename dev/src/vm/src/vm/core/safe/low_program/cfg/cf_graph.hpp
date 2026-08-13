/**
 * @file cf_graph.hpp
 * @brief Control-flow graph representation for lowered VM functions.
 */
#pragma once

#include "../instruction.hpp"

#include <vm/bytecode/bytecode.hpp>

#include <array>
#include <string>
#include <vector>

namespace vm::low {
	// Reintroduce the alias to avoid circular dependency with `low_program.hpp`.
	using MicroBytecode = std::vector<MicroInstruction>;

	namespace cf {
		/**
		 * @brief Identifier of a basic block in the control-flow graph.
		 */
		using BasicBlockID = usize;

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

			OutEdges()                           = default;
			OutEdges(const OutEdges&)            = default;
			OutEdges& operator=(const OutEdges&) = default;

			/**
			 * @brief Returns the number of outgoing edges.
			 */
			[[nodiscard]] usize size() const;

			/**
			 * @brief Returns the edge kind.
			 */
			[[nodiscard]] Kind kind() const;

			/**
			 * @brief Configures a conditional edge pair.
			 * @param kind Conditional opcode kind.
			 * @param target1 Target reached when condition succeeds.
			 * @param target2 Target reached when condition fails.
			 */
			void setCond(Kind kind, BasicBlockID target1, BasicBlockID target2);

			/**
			 * @brief Configures a single default edge.
			 * @param target Fallthrough or unconditional jump destination.
			 */
			void setDefault(BasicBlockID target);

			/**
			 * @brief Returns the default successor.
			 */
			[[nodiscard]] BasicBlockID next() const;

			/**
			 * @brief Returns the success successor for conditional branches.
			 */
			[[nodiscard]] BasicBlockID successTarget() const;

			/**
			 * @brief Returns the failure successor for conditional branches.
			 */
			[[nodiscard]] BasicBlockID failTarget() const;

			/**
			 * @brief Returns a mutable reference to the edge target at the given position.
			 * @param index Edge index in [0, size()).
			 */
			BasicBlockID& operator[](usize index);

			/**
			 * @brief Returns an immutable reference to the edge target at the given position.
			 * @param index Edge index in [0, size()).
			 */
			const BasicBlockID& operator[](usize index) const;

			/**
			 * @brief Returns a compact string representation for debugging.
			 */
			[[nodiscard]] std::string toString() const;

		private:
			std::array<BasicBlockID, 2> to        = { 0, 0 };
			usize                       no_edges  = 0;
			Kind                        edge_kind = Kind::End;
		};

		/**
		 * @brief Basic block metadata used by control-flow analyses.
		 */
		struct BasicBlock {
			const BasicBlockID id;
			const usize        start;
			const usize        end;
			OutEdges           succ;
			i64                ret_value;  // Value only relevant for blocks without outgoing edges.

			/**
			 * @brief Creates a basic block descriptor.
			 * @param id Numeric block identifier.
			 * @param start Inclusive instruction index where the block starts.
			 * @param end Exclusive instruction index where the block ends.
			 */
			BasicBlock(BasicBlockID id, usize start, usize end);

			BasicBlock() = delete;

			[[nodiscard]] std::ranges::range auto instructions(const vm::low::MicroBytecode& bc
			) const {
				return std::span(bc).subspan(start, end - start);
			}

			/**
			 * @brief Sets return value to block to specified value.
			 */
			void setRetValue(i64 value);

			/**
			 * @brief Returns the shape of outgoing edges.
			 */
			[[nodiscard]] OutEdges::Kind edgeKind() const;

			/**
			 * @brief Returns the default successor.
			 */
			[[nodiscard]] BasicBlockID next() const;

			/**
			 * @brief Returns the success successor for conditional branches.
			 */
			[[nodiscard]] BasicBlockID successTarget() const;

			/**
			 * @brief Returns the failure successor for conditional branches.
			 */
			[[nodiscard]] BasicBlockID failTarget() const;

			[[nodiscard]] bool isFallthrough() const;

			/**
			 * @brief Sets conditional successors.
			 * @param kind Conditional edge kind.
			 * @param target1 Success destination.
			 * @param target2 Failure destination.
			 */
			void setCondEdge(OutEdges::Kind kind, BasicBlockID target1, BasicBlockID target2);

			/**
			 * @brief Sets a single default successor.
			 * @param target Default destination.
			 */
			void setDefaultEdge(BasicBlockID target);

			/**
			 * @brief Returns the edge target at index.
			 * @param index Edge index in [0, edgeCount()).
			 */
			[[nodiscard]] BasicBlockID edge(usize index) const;

			/**
			 * @brief Returns the number of outgoing edges.
			 */
			[[nodiscard]] usize edgeCount() const;

			/**
			 * @brief Returns a compact string representation for debugging.
			 */
			[[nodiscard]] std::string toString() const;
		};

		/**
		 * @brief Control-flow graph built from lowered function bytecode.
		 */
		class ControlFlowGraph {
		private:
			std::vector<BasicBlock> blocks;

			/**
			 * @brief Builds graph blocks and edges from block beginnings.
			 * @param bc Micro-bytecode of lowered function.
			 * @param block_beginnings Sorted block start instruction offsets.
			 */
			void createFuncCFG(
				const low::MicroBytecode& bc, const std::vector<usize>& block_beginnings
			);

		public:
			ControlFlowGraph() = default;

			/**
			 * @brief Creates a control-flow graph from lowered function data.
			 * @param bc Micro-bytecode of lowered function to analyze.
			 */
			ControlFlowGraph(const low::MicroBytecode& bc);

			/**
			 * @brief Returns whether the graph is empty.
			 */
			[[nodiscard]] bool empty() const;

			/**
			 * @brief Returns number of blocks in the graph.
			 */
			[[nodiscard]] usize size() const;

			[[nodiscard]] std::ranges::range auto getBlocks() const { return std::span(blocks); }

			/**
			 * @brief Returns block metadata by identifier.
			 * @param id Block identifier.
			 */
			[[nodiscard]] const BasicBlock& getBlock(BasicBlockID id) const;

			/**
			 * @brief Builds an induced subgraph with an entry block containing only chosen blocks.
			 * @param entry_block_id Block that becomes the new entry (subgraph block 0).
			 * @param other_block_ids Block ids to keep in the resulting graph.
			 * @param dummy_exit_blocks If true, external edges are redirected to synthetic dummy
			 * blocks. If false, all edges must point to blocks in the subgraph.
			 * @return A standalone CFG representing the created induced subgraph.
			 */
			[[nodiscard]] ControlFlowGraph inducedSubgraph(
				BasicBlockID                     entry_block_id,
				const std::vector<BasicBlockID>& other_block_ids,
				bool                             dummy_exit_blocks
			) const;

			/**
			 * @brief Returns a multi-line string representation of this CFG for debugging.
			 */
			[[nodiscard]] std::string toString() const;
		};
	}  // vm::low::cf

}  // vm::low

/**
 * @file cf_graph.cpp
 * @brief Implementation of control-flow graph construction.
 */
#include "cf_graph.hpp"

#include "cf_analysis.hpp"

namespace vm::low::cf {

	OutEdges::OutEdges(): to{ 0, 0 } {}

	usize OutEdges::size() const { return no_edges; }

	OutEdges::Kind OutEdges::kind() const { return op_type; }

	void OutEdges::setCond(Kind kind, BasicBlockID target1, BasicBlockID target2) {
		CORE_ASSERT(no_edges == 0, "Outgoing edges already set for this basic block");
		this->op_type = kind;
		to[0]         = target1;
		to[1]         = target2;
		no_edges      = 2;
	}

	void OutEdges::setDefault(BasicBlockID target) {
		CORE_ASSERT(no_edges == 0, "Outgoing edges already set for this basic block");
		this->op_type = Kind::Default;
		to[0]         = target;
		no_edges      = 1;
	}

	BasicBlockID OutEdges::next() const {
		CORE_ASSERT(op_type == Kind::Default, "This block has no default outgoing edge.");
		return to[0];
	}

	BasicBlockID OutEdges::successTarget() const {
		CORE_ASSERT(
			op_type == Kind::JmpIf || op_type == Kind::JmpIfNot,
			"This block does not have conditional outgoing edges."
		);
		return to[0];
	}

	BasicBlockID OutEdges::failTarget() const {
		CORE_ASSERT(
			op_type == Kind::JmpIf || op_type == Kind::JmpIfNot,
			"This block does not have conditional outgoing edges."
		);
		return to[1];
	}

	BasicBlockID& OutEdges::operator[](usize index) {
		CORE_ASSERT(index < size(), "edge index out of bounds");
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
		return to[index];
	}

	const BasicBlockID& OutEdges::operator[](usize index) const {
		CORE_ASSERT(index < size(), "edge index out of bounds");
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
		return to[index];
	}

	BasicBlock::BasicBlock(BasicBlockID id, usize start, usize end):
		  succ(),
		  id(id),
		  start(start),
		  end(end) {}

	OutEdges::Kind BasicBlock::edgeKind() const { return succ.kind(); }

	BasicBlockID BasicBlock::next() const { return succ.next(); }

	BasicBlockID BasicBlock::successTarget() const { return succ.successTarget(); }

	BasicBlockID BasicBlock::failTarget() const { return succ.failTarget(); }

	void BasicBlock::setCondEdge(OutEdges::Kind kind, BasicBlockID target1, BasicBlockID target2) {
		succ.setCond(kind, target1, target2);
	}

	void BasicBlock::setDefaultEdge(BasicBlockID target) { succ.setDefault(target); }

	BasicBlockID BasicBlock::edge(usize index) const { return succ[index]; }

	usize BasicBlock::edgeCount() const { return succ.size(); }

	ControlFlowGraph::ControlFlowGraph(const low::MicroBytecode& bc) {
		createCFG(bc, basicBlockBeginnings(bc));
	}

	usize ControlFlowGraph::size() const { return blocks.size(); }

	const BasicBlock& ControlFlowGraph::getBlock(BasicBlockID id) const {
		CORE_ASSERT(id < blocks.size(), "Invalid block ID");
		return blocks[id];
	}

	void ControlFlowGraph::createCFG(
		const low::MicroBytecode& bc, const std::vector<usize>& block_beginnings
	) {
		blocks.clear();
		blocks.reserve(block_beginnings.size());

		CORE_ASSERT(
			!block_beginnings.empty(),
			"There should be at least one block beginning for a valid function"
		);

		auto instr_to_block = [block_beginnings](usize instr_index) {
			auto it = std::ranges::lower_bound(block_beginnings, instr_index);

			CORE_ASSERT(
				it != block_beginnings.end(), "Instruction index out of bounds for block beginnings"
			);

			auto block_idx = std::distance(block_beginnings.begin(), it);
			return static_cast<usize>(block_idx);
		};

		for (BasicBlockID id = 0; id < block_beginnings.size() - 1; ++id) {
			usize start = block_beginnings[id];
			usize end   = block_beginnings[id + 1];
			blocks.emplace_back(id, start, end);
		}
		blocks.emplace_back(block_beginnings.size() - 1, block_beginnings.back(), bc.size());

		for (const auto& block: blocks) {
			const vm::MicroInstruction& last_instr  = bc[block.end - 1];
			low::MicroOpcode            last_opcode = getInstructionOpcode(last_instr);

			switch (last_opcode) {
			case low::MicroOpcode::jmp_label: {
				const usize jump_target = jumpTarget(block.end, last_instr.arg0);
				CORE_ASSERT(jump_target < bc.size(), "Jump target points past last instruction");
				usize target_block_idx = instr_to_block(jump_target);
				blocks[block.id].setDefaultEdge(target_block_idx);
				break;
			}
			case low::MicroOpcode::jmpIf_label: {
				const usize jump_target = jumpTarget(block.end, last_instr.arg0);
				CORE_ASSERT(jump_target < bc.size(), "Jump target points past last instruction");
				usize jmp_target = instr_to_block(jump_target);
				blocks[block.id].setCondEdge(OutEdges::Kind::JmpIf, jmp_target, block.id + 1);
				break;
			}
			case low::MicroOpcode::jmpIfNot_label: {
				const usize jump_target = jumpTarget(block.end, last_instr.arg0);
				CORE_ASSERT(jump_target < bc.size(), "Jump target points past last instruction");
				usize jmp_target = instr_to_block(jump_target);
				blocks[block.id].setCondEdge(OutEdges::Kind::JmpIfNot, jmp_target, block.id + 1);
				break;
			}
			case low::MicroOpcode::ret:
			case low::MicroOpcode::ret_tailcall_func: {
				// No outgoing edges from return blocks
				break;
			}
			default: {
				if (block.end < bc.size()) {
					// If the block does not end with a jump, add a default edge to the next block
					blocks[block.id].setDefaultEdge(block.id + 1);
				}
				break;
			}
			}
		}
	}

	ControlFlowGraph ControlFlowGraph::subgraph(const std::vector<BasicBlockID>& block_ids) const {
		ControlFlowGraph subgraph;
		subgraph.blocks.reserve(block_ids.size());

		auto dummy_block_id = static_cast<BasicBlockID>(subgraph.blocks.size());

		std::vector<BasicBlockID> old_to_new(blocks.size() + 1, dummy_block_id);

		for (BasicBlockID old_id: block_ids) {
			CORE_ASSERT(old_id < blocks.size(), "Invalid block ID in subgraph request");
			CORE_ASSERT(
				old_to_new[old_id] == dummy_block_id, "Duplicate block ID in subgraph request"
			);

			const auto new_id  = static_cast<BasicBlockID>(subgraph.blocks.size());
			old_to_new[old_id] = new_id;

			const BasicBlock& src = blocks[old_id];
			subgraph.blocks.emplace_back(new_id, src.start, src.end);
		}
		// Add dummy block to redirect edges that go outside the selected subset
		// PLACEHOLDER: replace with logic which creates a different dummy for each jmp or smth
		auto ret_instr_pos = blocks.back().end - 1;
		subgraph.blocks.emplace_back(dummy_block_id, ret_instr_pos, ret_instr_pos + 1);

		for (BasicBlockID old_id: block_ids) {
			const BasicBlockID new_id = old_to_new[old_id];
			const BasicBlock&  src    = blocks[old_id];
			BasicBlock&        dst    = subgraph.blocks[new_id];

			dst.succ = src.succ;
			for (usize i = 0; i < src.edgeCount(); ++i) {
				BasicBlockID old_target_id = src.edge(i);
				BasicBlockID new_target_id = old_to_new[old_target_id];
				dst.succ[i]                = new_target_id;
			}
		}

		return subgraph;
	}
}  // namespace vm::low::cf

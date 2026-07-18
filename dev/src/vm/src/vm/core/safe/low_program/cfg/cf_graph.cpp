/**
 * @file cf_graph.cpp
 * @brief Implementation of control-flow graph construction.
 */
#include "cf_graph.hpp"

#include "cf_analysis.hpp"

#include <sstream>
#include <string>

namespace vm::low::cf {
	usize OutEdges::size() const { return no_edges; }

	OutEdges::Kind OutEdges::kind() const { return edge_kind; }

	void OutEdges::setCond(Kind kind, BasicBlockID target1, BasicBlockID target2) {
		CORE_ASSERT(no_edges == 0, "Outgoing edges already set for this basic block");
		this->edge_kind = kind;
		to[0]           = target1;
		to[1]           = target2;
		no_edges        = 2;
	}

	void OutEdges::setDefault(BasicBlockID target) {
		CORE_ASSERT(no_edges == 0, "Outgoing edges already set for this basic block");
		this->edge_kind = Kind::Default;
		to[0]           = target;
		no_edges        = 1;
	}

	BasicBlockID OutEdges::next() const {
		CORE_ASSERT(edge_kind == Kind::Default, "This block has no default outgoing edge.");
		return to[0];
	}

	BasicBlockID OutEdges::successTarget() const {
		CORE_ASSERT(
			edge_kind == Kind::JmpIf || edge_kind == Kind::JmpIfNot,
			"This block does not have conditional outgoing edges."
		);
		return to[0];
	}

	BasicBlockID OutEdges::failTarget() const {
		CORE_ASSERT(
			edge_kind == Kind::JmpIf || edge_kind == Kind::JmpIfNot,
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
		  id(id),
		  start(start),
		  end(end),
		  succ(),
		  ret_value(0) {}

	void BasicBlock::setRetValue(i64 value) { ret_value = value; }

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
		createFuncCFG(bc, basicBlockBeginnings(bc));
	}

	bool ControlFlowGraph::empty() const { return blocks.empty(); }

	usize ControlFlowGraph::size() const { return blocks.size(); }

	const BasicBlock& ControlFlowGraph::getBlock(BasicBlockID id) const {
		CORE_ASSERT(id < blocks.size(), "Invalid block ID");
		return blocks[id];
	}

	void ControlFlowGraph::createFuncCFG(
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

		// Remove unreachable blocks
		std::vector<BasicBlockID> reachable{ 0 };
		std::vector<bool> visited(blocks.size(), false);
		visited[0] = true;
		usize stack_ptr = 0;

		while (stack_ptr < reachable.size()) {
			const auto curr = reachable[stack_ptr++];

			for (usize i = 0; i < blocks[curr].edgeCount(); ++i) {
				BasicBlockID v = blocks[curr].edge(i);
				if (!visited[v]) {
					visited[v] = true;
					reachable.push_back(v);
				}
			}
		}

		*this = inducedSubgraph(0, reachable, false);
	}

	ControlFlowGraph ControlFlowGraph::inducedSubgraph(
		BasicBlockID entry_block_id,
		const std::vector<BasicBlockID>& other_block_ids,
		bool dummy_exit_blocks
	) const {
		ControlFlowGraph subgraph;
		subgraph.blocks.emplace_back(0, blocks[entry_block_id].start, blocks[entry_block_id].end);

		const auto undefined_id = static_cast<BasicBlockID>(-1);

		std::vector<BasicBlockID> old_block_ids = { entry_block_id };
		std::vector<BasicBlockID> old_to_new_id(blocks.size(), undefined_id);
		old_to_new_id[entry_block_id] = 0;

		for (BasicBlockID old_id: other_block_ids) {
			CORE_ASSERT(old_id < blocks.size(), "Invalid block ID in subgraph request");
			if (old_to_new_id[old_id] != undefined_id) continue;

			const auto new_id     = static_cast<BasicBlockID>(subgraph.blocks.size());
			old_to_new_id[old_id] = new_id;

			subgraph.blocks.emplace_back(new_id, blocks[old_id].start, blocks[old_id].end);
			old_block_ids.push_back(old_id);
		}

		const auto entry_pos = static_cast<i64>(subgraph.blocks[0].start);

		for (BasicBlockID old_id: old_block_ids) {
			const BasicBlockID new_id = old_to_new_id[old_id];
			const BasicBlock*  src    = &blocks[old_id];
			BasicBlock*        dst    = &subgraph.blocks[new_id];

			dst->succ = src->succ;
			for (usize i = 0; i < src->edgeCount(); ++i) {
				BasicBlockID old_target_id = src->edge(i);
				BasicBlockID new_target_id = old_to_new_id[old_target_id];
				if (new_target_id == undefined_id) {
					CORE_ASSERT(
						dummy_exit_blocks,
						"Induced subgraph contains an edge to a block not in the subgraph "
						"while dummy exit blocks are disabled"
					);
					auto jmp_dest_pos = static_cast<i64>(blocks[old_target_id].start);

					// Create empty exit block representing this outgoing edge
					new_target_id = static_cast<BasicBlockID>(subgraph.blocks.size());
					subgraph.blocks.emplace_back(new_target_id, jmp_dest_pos, jmp_dest_pos);
					subgraph.blocks.back().setRetValue(jmp_dest_pos - entry_pos);

					// Update mapping to avoid creating multiple identical dummy blocks
					old_to_new_id[old_target_id] = new_target_id;

					// We must refresh the pointer, as emplace_back invalidates all references
					dst = &subgraph.blocks[new_id];
				}
				dst->succ[i] = new_target_id;
			}
		}

		return subgraph;
	}

	/* Debug string conversions */

	std::string OutEdges::toString() const {
		std::ostringstream oss;
		switch (edge_kind) {
		case Kind::End:
			oss << "End{}";
			break;
		case Kind::Default:
			oss << "Default{next -> " << to[0] << "}";
			break;
		case Kind::JmpIf:
			oss << "JmpIf{condition: T -> " << to[0] << ", F -> " << to[1] << "}";
			break;
		case Kind::JmpIfNot:
			oss << "JmpIfNot{condition: T -> " << to[1] << ", F -> " << to[0] << "}";
			break;
		}
		return oss.str();
	}

	bool BasicBlock::isFallthrough() const {
		return edgeKind() == OutEdges::Kind::Default && next() == id + 1;
	}

	std::string BasicBlock::toString() const {
		std::ostringstream oss;
		oss << "Block{bid = " << id << ", range = [" << start << ", " << end
			<< "), ret = " << ret_value << ", edge = " << succ.toString() << "}";
		return oss.str();
	}

	std::string ControlFlowGraph::toString() const {
		std::ostringstream oss;
		oss << "ControlFlowGraph{blocks = [\n";
		for (usize i = 0; i < blocks.size(); ++i) {
			oss << "  " << blocks[i].toString();
			if (i + 1 < blocks.size()) oss << ",\n";
		}
		oss << "\n]}";
		return oss.str();
	}
}  // namespace vm::low::cf

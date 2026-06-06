#include "../jit_compiler.hpp"
#include "memory/memory.hpp"
#include "stencil_holder.hpp"

#include <base/config/build_type.hpp>

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

#include <functional>

namespace vm::jit {
	cnp::JitFuncMemory compileCP(
		const vm::low::cf::ControlFlowGraph& cfg, const vm::low::MicroBytecode& bc
	) {
		using namespace cnp;
		using namespace std::views;

		auto transform_opcode = [](low::MicroOpcode opcode) {
			switch (opcode) {
			case low::MicroOpcode::virtual_call_pptr_method:
				[[fallthrough]];
			case low::MicroOpcode::call_func:
				[[fallthrough]];
			case low::MicroOpcode::jit_call_entrypoint: {
				return std::to_underlying(SpecialStencils::Trampoline);
			}
			default: {
			}
			}

			switch (opcode) {
#define HANDLE_NONJITABLE_INSTR(instr) \
	case low::MicroOpcode::instr:      \
		return std::to_underlying(SpecialStencils::CallAddr);
#include "../non_jitable_def.hpp"
#undef HANDLE_NONJITABLE_INSTR
			default:
				return std::to_underlying(opcode);
			}
		};

		auto choose_edge = [](auto block) -> SpecialStencils {
			switch (block.edgeKind()) {
			case vm::low::cf::OutEdges::Kind::JmpIf:
				return SpecialStencils::JumpIf;
			case vm::low::cf::OutEdges::Kind::JmpIfNot:
				return SpecialStencils::JumpIfNot;
			case vm::low::cf::OutEdges::Kind::End:
				return SpecialStencils::Return;
			case vm::low::cf::OutEdges::Kind::Default:
				return SpecialStencils::Jump;
			}
		};

		auto get_opfunc_size = [&](u64 opcode) -> u64 { return stencilsData().at(opcode).size; };

		std::vector<usize> block_offsets;
		block_offsets.reserve(cfg.size() + 1);
		block_offsets.push_back(0);
		for (const low::cf::BasicBlock& block: cfg.getBlocks()) {
			std::cerr << block_offsets.back() << ", ";
			block_offsets.push_back(block_offsets.back());
			usize& current_offset = block_offsets.back();
			for (MicroInstruction instr: block.instructions(bc)) {
				auto opcode = getInstructionOpcode(instr);
				if (low::isOpcodeNonExecutable(opcode)) continue;
				current_offset += get_opfunc_size(transform_opcode((opcode)));
			}
			auto stencil = std::to_underlying(choose_edge(block));
			current_offset += get_opfunc_size(stencil);
		}


		auto  memory = JitFuncMemory::allocate(block_offsets.back());
		byte* next   = memory.addr;

		auto patch_stencil = [&](u64 opcode, auto func) {
			auto stencil_data = stencilsData().at(opcode);
			auto previous     = next;
			next              = relocate(stencil_data, previous);
			stencil_data.patch(previous, func);
		};

		for (auto [idx, block]: std::views::enumerate(cfg.getBlocks())) {
			CORE_ASSERT(
				next == memory.addr + block_offsets[idx],
				"idx: ",
				idx,
				" next: ",
				next - memory.addr,
				" expected: ",
				block_offsets[idx]
			);
			for (MicroInstruction instr: block.instructions(bc)) {
				auto opcode = getInstructionOpcode(instr);
				if (low::isOpcodeNonExecutable(opcode)) continue;

				patch_stencil(transform_opcode(opcode), [&instr, &next, &opcode](HoleValue value) {
					switch (value) {
					case HoleValue::Arg0:
						return instr.arg0;
					case HoleValue::Arg1:
						return instr.arg1;
					case HoleValue::ContinueFn:
						return std::bit_cast<u64>(next);
					case HoleValue::CallFn:
						return std::bit_cast<u64>(
							vm::OpFuns::DEBUG_OPFUNS[std::to_underlying(opcode)]
						);
					case HoleValue::JmpFn:
						CORE_PANIC("jumping inside a basic block");
					case HoleValue::Zero:
						CORE_PANIC("zero left as a hole");
					}
				});
			}
			auto stencil = std::to_underlying(choose_edge(block));
			patch_stencil(stencil, [&](HoleValue value) {
				switch (value) {
				case HoleValue::Arg0:
					CORE_PANIC("arg0 passed to stencil jump");
				case HoleValue::Arg1:
					CORE_PANIC("arg1 passed to stencil jump");
				case HoleValue::ContinueFn:
					return std::bit_cast<u64>(memory.addr + block_offsets.at(block.edge(1)));
				case HoleValue::CallFn:
					CORE_PANIC("call function passed to stencil jump");
				case HoleValue::JmpFn:
					return std::bit_cast<u64>(memory.addr + block_offsets.at(block.edge(0)));
				case HoleValue::Zero:
					CORE_PANIC("zero left as a hole");
				}
			});
		}

		IF_BUILD_TYPE_DEV(memory.dump("compiled function"));
		memory.markExecutable();
		return memory;
	}
}

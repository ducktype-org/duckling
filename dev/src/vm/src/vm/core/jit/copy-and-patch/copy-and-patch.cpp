#include "../jit_compiler.hpp"
#include "../jit_helper.hpp"
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
#define HANDLE_NONJITABLE_INSTR(instr) \
	case low::MicroOpcode::instr:      \
		return std::to_underlying(SpecialStencils::CallAddr);
#include "../non_jitable_def.hpp"
#undef HANDLE_NONJITABLE_INSTR
			default:
				return std::to_underlying(opcode);
			}
		};

		auto choose_edge = [](auto block) -> base::Optional<SpecialStencils> {
			if (block.isFallthrough()) return std::nullopt;
			switch (block.edgeKind()) {
			case vm::low::cf::OutEdges::Kind::JmpIf:
				return SpecialStencils::JumpIf;
			case vm::low::cf::OutEdges::Kind::JmpIfNot:
				return SpecialStencils::JumpIfNot;
			case vm::low::cf::OutEdges::Kind::End:
				return std::nullopt;
			case vm::low::cf::OutEdges::Kind::Default:
				return SpecialStencils::Jump;
			}
		};

		auto get_opfunc_size = [&](u64 opcode) -> u64 { return stencilsData().at(opcode).size; };

		std::vector<usize> block_offsets;
		block_offsets.reserve(cfg.size() + 1);
		block_offsets.push_back(0);
		for (const low::cf::BasicBlock& block: cfg.getBlocks()) {
			block_offsets.push_back(block_offsets.back());
			usize& current_offset = block_offsets.back();
			for (const MicroInstruction& instr: block.instructions(bc)) {
				auto opcode = getInstructionOpcode(instr);
				if (low::isOpcodeNonExecutable(opcode)) continue;
				current_offset += get_opfunc_size(transform_opcode((opcode)));
			}

			if (auto stencil = choose_edge(block))
				current_offset += get_opfunc_size(std::to_underlying(stencil.value()));
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

			for (const MicroInstruction& instr: block.instructions(bc)) {
				auto opcode = getInstructionOpcode(instr);
				if (low::isOpcodeNonExecutable(opcode)) continue;

				patch_stencil(transform_opcode(opcode), [&](HoleValue value) {
					switch (value) {
					case HoleValue::InstrPtr:
						return std::bit_cast<u64>(&instr);
					case HoleValue::Arg0:
						return instr.arg0;
					case HoleValue::Arg1:
						return instr.arg1;
					case HoleValue::ContinueFn:
						return std::bit_cast<u64>(next);
					case HoleValue::CallFn:
						if (opcode == low::MicroOpcode::call_func
						    || opcode == low::MicroOpcode::virtual_call_pptr_method) {
							return std::bit_cast<u64>(&jit::helpers::trampoline);
						} else {
							return std::bit_cast<u64>(
								// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
								vm::OpFuns::DEBUG_OPFUNS[std::to_underlying(opcode)]
							);
						}
					case HoleValue::CallOpcode:
						return *reinterpret_cast<const u64*>(&instr);
					case HoleValue::None:
						[[fallthrough]];
					case HoleValue::JmpFn:
						[[fallthrough]];
					case HoleValue::COUNT:
						CORE_PANIC(base::enumToStr(value), " was left as a hole in a basic block");
					}
				});
			}
			if (auto stencil = choose_edge(block)) {
				patch_stencil(std::to_underlying(stencil.value()), [&](HoleValue value) {
					switch (value) {
					case HoleValue::ContinueFn:
						return std::bit_cast<u64>(memory.addr + block_offsets.at(block.edge(1)));
					case HoleValue::JmpFn:
						return std::bit_cast<u64>(memory.addr + block_offsets.at(block.edge(0)));
					case HoleValue::InstrPtr:
						[[fallthrough]];
					case HoleValue::Arg0:
						[[fallthrough]];
					case HoleValue::Arg1:
						[[fallthrough]];
					case HoleValue::CallOpcode:
						[[fallthrough]];
					case HoleValue::CallFn:
						[[fallthrough]];
					case HoleValue::None:
						[[fallthrough]];
					case HoleValue::COUNT:
						CORE_PANIC(base::enumToStr(value), " was passed to jump stencil");
					}
				});
			}
		}

		IF_BUILD_TYPE_DEV(memory.dump("compiled_function"));
		memory.markExecutable();
		return memory;
	}
}

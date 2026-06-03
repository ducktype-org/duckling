#include "../jit_compiler.hpp"
#include "memory/memory.hpp"
#include "stencil_holder.hpp"

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <base/config/build_type.hpp>

#include <iostream>

namespace vm::jit {
	cnp::JitFuncMemory compileCP(const vm::low::LowFuncData& func_data) {
		using namespace cnp;
		auto get_opfunc_size
			= [&](auto opcode) { return stencilsData().at(std::to_underlying(opcode)).size; };
		usize size = std::ranges::fold_left(
						 func_data.bc | std::views::transform(getInstructionOpcode)
							 | std::views::transform(get_opfunc_size),
						 0,
						 std::plus{}
					 )
		           + get_opfunc_size(SpecialStencils::ret);

		auto  memory = JitFuncMemory::allocate(size);
		byte* next   = memory.addr;

		auto patch_stencil = [&](auto opcode, auto func) {
			auto stencil_data = stencilsData().at(std::to_underlying(opcode));
			auto previous     = next;
			next              = relocate(stencil_data, previous);
			stencil_data.patch(previous, func);
		};

		for (auto instr: func_data.bc) {
			auto opcode = getInstructionOpcode(instr);
			if (low::isOpcodeNonExecutable(opcode)) {
				continue;
			}

			// std::cerr << vm::low::OPCODE_NAMES[std::to_underlying(opcode)] << ": "
			//		  << std::to_underlying(opcode) << std::endl;
			patch_stencil(opcode, [&instr, &next](HoleValue value) {
				switch (value) {
				case HoleValue::ARG0:
					return instr.arg0;
				case HoleValue::ARG1:
					return instr.arg1;
				case HoleValue::CONTINUE_FUNCTION:
					return std::bit_cast<u64>(next);
				default:
					CORE_PANIC("unknown hole value");
					return 0ul;
				}
			});
		}
		patch_stencil(SpecialStencils::ret, [](HoleValue) -> u64 {
			CORE_PANIC("Special stencil 'ret' has a relocation");
		});

		IF_BUILD_TYPE_DEV(memory.dump("compiled function"));
		memory.markExecutable();
		return memory;
	}
}

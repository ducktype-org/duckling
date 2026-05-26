#include "../jit_compiler.hpp"
#include "memory/memory.hpp"
#include "stencil_holder.hpp"

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/low_program.hpp>

namespace vm::jit {
	cnp::JitFuncMemory compileCP(const vm::low::LowFuncData& func_data) {
		using namespace cnp;
		auto get_opfunc_size = [&](low::MicroOpcode opcode) {
			return stencilsData().at(static_cast<u64>(opcode)).size;
		};
		usize size = std::ranges::fold_left(
			func_data.bc | std::views::transform(getInstructionOpcode)
				| std::views::transform(get_opfunc_size),
			0,
			std::plus{}
		);

		auto  memory = JitFuncMemory::allocate(size);
		byte* next   = memory.addr;
		for (auto instr: func_data.bc) {
			auto opcode = getInstructionOpcode(instr);

			auto stencil_data = stencilsData().at(static_cast<u64>(opcode));
			auto previous     = next;
			next              = relocate(stencil_data, previous);
			stencil_data.patch(previous, [&instr, &next](HoleValue value) {
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
		return memory;
	}
}

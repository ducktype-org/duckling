#include "../jit_compiler.hpp"
#include "memory/memory.hpp"
#include "stencil_holder.hpp"

#include <base/config/build_type.hpp>

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/low_program.hpp>

#include <functional>

namespace vm::jit {
	cnp::JitFuncMemory compileCP(const vm::low::LowFuncData& func_data) {
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
				return std::to_underlying(opcode);
			}
			}
		};

		auto get_opfunc_size = [&](u64 opcode) -> u64 { return stencilsData().at(opcode).size; };

		u64 size = std::ranges::fold_left(
					   func_data.bc | transform(getInstructionOpcode)
						   | filter(std::not_fn(low::isOpcodeNonExecutable))
						   | transform(transform_opcode) | transform(get_opfunc_size),
					   0,
					   std::plus{}
				   )
		         + get_opfunc_size(std::to_underlying(SpecialStencils::Ret));

		auto  memory = JitFuncMemory::allocate(size);
		byte* next   = memory.addr;

		auto patch_stencil = [&](u64 opcode, auto func) {
			auto stencil_data = stencilsData().at(opcode);
			auto previous     = next;
			next              = relocate(stencil_data, previous);
			stencil_data.patch(previous, func);
		};

		for (auto instr: func_data.bc) {
			auto opcode = getInstructionOpcode(instr);
			if (low::isOpcodeNonExecutable(opcode)) continue;

			patch_stencil(transform_opcode(opcode), [&instr, &next](HoleValue value) {
				switch (value) {
				case HoleValue::Arg0:
					return instr.arg0;
				case HoleValue::Arg1:
					return instr.arg1;
				case HoleValue::ContinueFunction:
					return std::bit_cast<u64>(next);
				default:
					CORE_PANIC("unknown hole value");
					return 0ul;
				}
			});
		}
		patch_stencil(std::to_underlying(SpecialStencils::Ret), [](HoleValue) -> u64 {
			CORE_PANIC("Special stencil 'ret' has a relocation");
		});

		IF_BUILD_TYPE_DEV(memory.dump("compiled function"));
		memory.markExecutable();
		return memory;
	}
}

#include "stencils/import_stencils.hpp"
#include "memory/memory.hpp"

#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/jit/jit_compiler.hpp>

namespace vm::jit {
	vm::JitOpFun* compileCP(const vm::low::LowFuncData& func_data) {

PUSH_DIAGNOSTIC ALLOW_EXTENSIONS
		static Stencils     stencils = Stencils{
				.binary = {
#embed "wrapper-text" suffix(, )
			}, .functions = {
#include "wrapper.nm"
			}
		}; // TODO: order by Opcode
POP_DIAGNOSTIC

		auto   opcodes = func_data.bc | views::transform(getInstructionOpcode);
		size_t size    = ranges::fold_left(opcodes);
		
		auto memory = JitMemory::allocate(size); 
		std::byte* next = memory.memory;

		auto add_instr = [&](std::string_view binary) {
			std::memcpy(next, binary.begin(), binary.size());
		};

		for (auto opcode : opcodes) {
			add_instr(stencils.function_binary(opcode));
		}
	}
}

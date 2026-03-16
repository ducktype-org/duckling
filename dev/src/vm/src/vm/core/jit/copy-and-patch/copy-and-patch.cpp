#include "memory/memory.hpp"
#include "stencils/import_stencils.hpp"

#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/thread/low_program/low_program.hpp>

namespace vm::jit::cnp {
	using JitOpFun = void(const vm::MicroInstruction**, byte**, vm::Frame**, vm::VMThread*);

	JitOpFun* compileCP(const vm::low::LowFuncData& func_data) {
		PUSH_DIAGNOSTIC       ALLOW_EXTENSIONS;
		static constexpr char _bin[] = {
#embed "wrapper-so" suffix(, )
			0
		};
		POP_DIAGNOSTIC

		constexpr static auto stencils = Stencils out{ .binary    = std::to_array(_bin),
			                                           .functions = {
#include <wrapper-nm>
													   } };

		auto opcodes         = func_data.bc | std::views::transform(getInstructionOpcode);
		auto get_opfunc_size = [&](low::MicroOpcode opcode) {
			return stencils.functions[static_cast<u64>(opcode)].size;
		};
		usize size = std::ranges::fold_left(
			opcodes | std::views::transform(get_opfunc_size), 0, std::plus{}
		);

		auto       memory = JitMemory::allocate(size);
		byte* next   = memory.memory;

		auto add_instr = [&](auto binary) {
			std::ranges::copy(binary, next);
			next += std::ranges::size(binary);
		};

		for (low::MicroOpcode opcode: opcodes)
			add_instr(stencils.stencil_binary(static_cast<u64>(opcode)));
		return memory.into_func<JitOpFun>();
	}
}

#include "memory/memory.hpp"
#include "stencils/import_stencils.hpp"

#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/thread/low_program/low_program.hpp>

namespace vm::jit::cnp {
	using JitOpFun = void(const vm::MicroInstruction**, byte**, vm::Frame**, vm::VMThread*);

	JitOpFun* compileCP(const vm::low::LowFuncData& func_data) {
		PUSH_DIAGNOSTIC
		ALLOW_EXTENSIONS
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
		static constexpr char BIN[] = {
#embed "wrapper-so" suffix(, )
		};
		POP_DIAGNOSTIC

		constexpr static auto STENCILS = Stencils out{ .binary    = std::to_array(BIN),
			                                           .functions = {
#include <wrapper-nm>
													   } };

		auto opcodes         = func_data.bc | std::views::transform(getInstructionOpcode);
		auto get_opfunc_size = [&](low::MicroOpcode opcode) {
			return STENCILS.functions[static_cast<u64>(opcode)].size;
		};
		usize size = std::ranges::fold_left(
			opcodes | std::views::transform(get_opfunc_size), 0, std::plus{}
		);

		auto  memory = JitFuncMemory::allocate(size);
		byte* next   = memory.addr;

		auto add_instr = [&](auto binary) {
			std::ranges::copy(binary, next);
			next += std::ranges::size(binary);
		};

		for (low::MicroOpcode opcode: opcodes)
			add_instr(STENCILS.stencilBinary(static_cast<u64>(opcode)));
		return memory.into_func<JitOpFun>();
	}
}

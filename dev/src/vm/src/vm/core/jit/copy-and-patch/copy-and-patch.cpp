#include "memory/memory.hpp"
#include "stencils/import_stencils.hpp"

#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/thread/low_program/low_program.hpp>

namespace vm::jit {
	using JitOpFun = void(const vm::MicroInstruction**, std::byte**, vm::Frame**, vm::VMThread*);

	JitOpFun* compileCP(const vm::low::LowFuncData& func_data) {
		PUSH_DIAGNOSTIC ALLOW_EXTENSIONS static constexpr char _bin[] = {
#embed "wrapper-text" suffix(, )
			0
		};
		POP_DIAGNOSTIC

		static Stencils stencils = [] {
			Stencils out{ .binary    = std::to_array(_bin),
				          .functions = {
#include "wrapper-nm"
						  } };

			constexpr std::string_view                         wrapper_prefix = "wrapper_";
			std::array<LLVM_nm_data, low::OPCODE_NAMES.size()> ordered{};

			for (const auto& fun: out.functions) {
				std::string_view symbol_name = fun.name;
				auto             prefix_pos  = symbol_name.find(wrapper_prefix);
				if (prefix_pos == std::string_view::npos) continue;

				auto opcode_in_symbol = symbol_name.substr(prefix_pos + wrapper_prefix.size());
				for (size_t opcode = 0; opcode < low::OPCODE_NAMES.size(); ++opcode) {
					std::string_view opcode_name = low::OPCODE_NAMES[opcode];
					if (!opcode_in_symbol.starts_with(opcode_name)) continue;
					ordered[opcode] = fun;
					break;
				}
			}

			for (size_t opcode = 0; opcode < low::OPCODE_NAMES.size(); ++opcode)
				out.functions[opcode] = ordered[opcode];
			return out;
		}();

		auto opcodes         = func_data.bc | std::views::transform(getInstructionOpcode);
		auto get_opfunc_size = [&](low::MicroOpcode opcode) {
			return stencils.functions[static_cast<u64>(opcode)].size;
		};
		size_t size = std::ranges::fold_left(
			opcodes | std::views::transform(get_opfunc_size), 0, std::plus{}
		);

		auto       memory = JitMemory::allocate(size);
		std::byte* next   = memory.memory;

		auto add_instr = [&](auto binary) {
			std::ranges::copy(binary, next);
			next += std::ranges::size(binary);
		};

		for (low::MicroOpcode opcode: opcodes)
			add_instr(stencils.stencil_binary(static_cast<u64>(opcode)));
		return memory.into_func<JitOpFun>();
	}
}

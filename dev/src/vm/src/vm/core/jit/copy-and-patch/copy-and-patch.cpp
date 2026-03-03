#include "memory/memory.hpp"
#include "stencils/import_stencils.hpp"

#include <vm/core/jit/jit_compiler.hpp>
#include <vm/core/thread/low_program/instruction.hpp>

namespace vm::jit {
	vm::JitOpFun* compileCP(const vm::low::LowFuncData& func_data) {
		static constexpr char _bin[] = {
#embed "wrapper-text" suffix(, )
		};
		PUSH_DIAGNOSTIC ALLOW_EXTENSIONS static Stencils stencils
			= [] {
				Stencils out{ .binary    = std::to_array(_bin),
				              .functions = {
#include "wrapper-nm"
							} };

				constexpr std::string_view wrapper_prefix = "wrapper_";
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
		POP_DIAGNOSTIC

		auto   opcodes = func_data.bc | std::views::transform(getInstructionOpcode);
		size_t size    = std::ranges::fold_left(
            opcodes | std::views::transform([&](low::MicroOpcode opcode) {
                return stencils.functions[static_cast<u64>(opcode)].size;
            }),
            0,
            std::plus<>{}
        );

		auto       memory = JitMemory::allocate(size);
		std::byte* next   = memory.memory;

		auto add_instr
			= [&](std::string_view binary) { std::memcpy(next, binary.begin(), binary.size()); };

		for (low::MicroOpcode opcode: opcodes)
			add_instr(stencils.function_binary(static_cast<u64>(opcode)));
		return nullptr;
	}
}

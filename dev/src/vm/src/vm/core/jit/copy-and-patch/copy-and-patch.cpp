// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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

	namespace {
		// Maps non-jittable opcodes to call_addr, other ones leaves unchanged.
		[[nodiscard]] i64 nonjittableToCallAddr(low::MicroOpcode opcode) {
			switch (opcode) {
#define HANDLE_NONJITTABLE_INSTR(instr) \
	case low::MicroOpcode::instr:       \
		return std::to_underlying(jit::cnp::SpecialStencils::CallAddr);
#include "../non_jittable.def.hpp"
#undef HANDLE_NONJITTABLE_INSTR
			default:
				return static_cast<i64>(std::to_underlying(opcode));
			}
		}

		// Maps a cf out edge kind to an optional special stencil jump.
		[[nodiscard]] base::Optional<jit::cnp::SpecialStencils> cfEdgeToStencil(
			const low::cf::BasicBlock& block
		) {
			if (block.isFallthrough()) return std::nullopt;
			switch (block.edgeKind()) {
			case low::cf::OutEdges::Kind::JmpIf:
				return jit::cnp::SpecialStencils::JumpIf;
			case low::cf::OutEdges::Kind::JmpIfNot:
				return jit::cnp::SpecialStencils::JumpIfNot;
			case low::cf::OutEdges::Kind::End:
				// @TODO: #3584 compileCP cannot return from a loop: an End edge gets no stencil,
				// so a compiled loop could not exit. Latent while compileCP is only called for
				// function entrypoints (op_jitFuncEntrypoint).
				return std::nullopt;
			case low::cf::OutEdges::Kind::Default:
				return jit::cnp::SpecialStencils::Jump;
			}
			return std::nullopt;
		}

		// Calculates offsets from the compiled functions start, to each block's end.
		[[nodiscard]] std::vector<usize> calculateBlockOffsets(
			const low::cf::ControlFlowGraph& cfg, const low::MicroBytecode& bc
		) {
			auto get_opfunc_size
				= [&](u64 opcode) -> u64 { return cnp::stencilsData().at(opcode).size; };

			std::vector<usize> block_offsets;
			block_offsets.reserve(cfg.size() + 1);
			block_offsets.push_back(0);
			for (const low::cf::BasicBlock& block: cfg.getBlocks()) {
				block_offsets.push_back(block_offsets.back());
				usize& current_offset = block_offsets.back();
				for (const MicroInstruction& instr: block.instructions(bc)) {
					auto opcode = getInstructionOpcode(instr);
					if (low::isOpcodeNonExecutable(opcode)) continue;
					current_offset += get_opfunc_size(nonjittableToCallAddr(opcode));
				}

				if (auto stencil = cfEdgeToStencil(block))
					current_offset += get_opfunc_size(std::to_underlying(stencil.value()));
			}
			return block_offsets;
		}
	}

	std::expected<cnp::JitFuncMemory, std::string> compileCP(
		const low::cf::ControlFlowGraph& cfg, const low::MicroBytecode& bc
	) {
		using namespace cnp;
		using namespace std::views;

		std::vector<usize> block_offsets = calculateBlockOffsets(cfg, bc);

		auto memory_result = JitFuncMemory::allocate(block_offsets.back());
		if (!memory_result) return std::unexpected{ std::move(memory_result).error() };
		auto memory = std::move(memory_result).value();

		byte* next          = memory.addr;
		auto  patch_stencil = [&](u64 opcode, auto func) {
            const auto& stencil_data = stencilsData().at(opcode);
            auto        previous     = next;
            next                     = relocate(stencil_data, previous);
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

				patch_stencil(nonjittableToCallAddr(opcode), [&](HoleValue value) {
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
								OpFuns::DEBUG_OPFUNS[std::to_underlying(opcode)]
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
			if (auto stencil = cfEdgeToStencil(block)) {
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

		CORE_DEV_LOG(
			DVMDetails,  // For future knowledge, this is where you print.
			(memory.dump("compiled_function.cnp"),
		     "Compiled function dumped to: compiled_function.cnp")
		);

		if (auto mark_result = memory.markExecutable(); !mark_result)
			return std::unexpected(std::move(mark_result).error());
		return memory;
	}
}

// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "eval/trap.hpp"

#include <base/pointers/box.hpp>

#include <vm/core/fast/program/instructions/executable.hpp>

#include <string>
#include <vector>

namespace vm::fast {
	namespace exec {
		struct ExecFunction;
		struct Instruction;
	}

	struct Frame final {
		bool flag = false;
		/// Instruction pointer, has to live in frame for easy function calls
		const exec::Instruction* ip = nullptr;

		byte* local_stack_base
			= nullptr;  /// Pointer to the start of the local stack for this frame.
	};

	class ThreadRuntimeState {
	public:
		ThreadRuntimeState()                                     = default;
		ThreadRuntimeState(const ThreadRuntimeState&)            = delete;
		ThreadRuntimeState(ThreadRuntimeState&&)                 = delete;
		ThreadRuntimeState& operator=(const ThreadRuntimeState&) = delete;
		ThreadRuntimeState& operator=(ThreadRuntimeState&&)      = delete;

		constexpr static usize LOCAL_STACK_SIZE = 8 * 1'024 * 1'024;  // 8 MB

	private:
		struct AlignedStackMemory final {
			alignas(8) std::array<byte, LOCAL_STACK_SIZE> data;
		};

		base::Box<AlignedStackMemory> local_stack_memory = makeBox<AlignedStackMemory>();

		// The frame count here is a heuristic approximation with no exact semantic meaning;
		// LOCAL_STACK_SIZE / sizeof(Frame) is simply used as an arbitrary, "large enough" size.
		std::vector<Frame> frame_stack = std::vector<Frame>(LOCAL_STACK_SIZE / sizeof(Frame));

		Frame* top_frame = frame_stack.data();

	public:
		byte* const local_stack_base
			= local_stack_memory->data.data();  /// Pointer to the start of the local stack.
		byte* const local_stack_end = local_stack_memory->data.data()
		                            + LOCAL_STACK_SIZE;  /// Pointer to the end of the local stack.

		// @TODO: #2729 This should be a valid pointer.
		byte* const global_data_buffer_base = nullptr;

		/// Set by a trapping handler before it jumps to `exit_instruction`.
		Trap        trap = Trap::None;
		std::string trap_message;

		/**
		 * The `exit` ending the running start function, where a trap jumps to. It is not a global
		 * constant on purpose: a known `exit` dispatcher makes GCC speculate every tail-call
		 * dispatch on it, doubling the dispatchers.
		 */
		const exec::Instruction* exit_instruction = nullptr;

		Frame* pushFrame(const exec::ExecFunction* function, byte* local_stack_base) {
			top_frame++;
			*top_frame = Frame{ .flag             = false,
				                .ip               = function->data.data(),
				                .local_stack_base = local_stack_base };
			return top_frame;
		}

		[[nodiscard]] bool canPopFrame() const noexcept { return top_frame > frame_stack.data(); }

		Frame* popFrame() noexcept {
			top_frame--;
			return top_frame;
		}

		/// Drops the frames a trap left behind.
		void resetFrames() noexcept { top_frame = frame_stack.data(); }

		[[nodiscard]] const exec::Instruction& currentInstruction() const { return *top_frame->ip; }

		constexpr void incrementInstruction(usize progress_by) { top_frame->ip += progress_by; }
	};
}

#pragma once

#include "base/pointers/box.hpp"

#include "vm/core/fast/program/instructions/executable.hpp"

#include <vector>

namespace vm::fast {
	namespace exec {
		struct ExecFunction;
		struct Instruction;
	}

	struct Frame {
		bool flag = false;
		/// Instruction pointer, has to live in frame for easy function calls
		const exec::Instruction* ip = nullptr;

		/// Pointer to the currently executing function, used for calls and returns.
		const exec::ExecFunction* func = nullptr;

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
		Box<std::array<byte, LOCAL_STACK_SIZE>> local_stack_memory
			= makeBox<std::array<byte, LOCAL_STACK_SIZE>>();

		std::vector<Frame> frame_stack = std::vector<Frame>(LOCAL_STACK_SIZE);

		Frame* top_frame = frame_stack.data();

	public:
		std::byte* const local_stack_base
			= local_stack_memory->data();  /// Pointer to the start of the local stack.
		std::byte* const local_stack_end
			= local_stack_memory->data()
		    + LOCAL_STACK_SIZE;  /// Pointer to the end of the local stack.

		// @TODO: #2729 This should be a valid pointer.
		std::byte* const global_data_buffer_base = nullptr;

		Frame* pushFrame(const exec::ExecFunction* function, byte* local_stack_base) {
			top_frame++;
			*top_frame = Frame{ .flag             = false,
				                .ip               = function->data.data(),
				                .func             = function,
				                .local_stack_base = local_stack_base };
			return top_frame;
		}

		void popFrame() { top_frame--; }

		[[nodiscard]] const exec::Instruction& currentInstruction() const { return *top_frame->ip; }

		constexpr void incrementInstruction(usize progress_by) { top_frame->ip += progress_by; }
	};
}

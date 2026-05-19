#pragma once

#include "base/pointers/box.hpp"

#include <deque>
#include <vector>

namespace vm::fast {
	namespace exec {
		struct ExecFunction;
		struct Instruction;
	}

	struct Frame {
		bool flag = false;
		/// Instruction pointer, has to live in frame for easy function calls
		const exec::Instruction* ip    = nullptr;

		void reset() { flag = false; ip = nullptr; }
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

		std::deque<Frame> frame_stack{ Frame() };

	public:
		Frame*                   top_frame = nullptr;


		std::byte* const local_stack_base
			= local_stack_memory->data();  /// Pointer to the start of the local stack.
		std::byte* const local_stack_end
			= local_stack_memory->data()
		    + LOCAL_STACK_SIZE;  /// Pointer to the end of the local stack.

		// @TODO: #2729 This should be a valid pointer.
		std::byte* const global_data_buffer_base = nullptr;

		void pushFrame() {
			frame_stack.emplace_back();
			top_frame = &frame_stack.back();
		}

		void popFrame() {
			frame_stack.pop_back();
			top_frame = &frame_stack.back();
		}
	};

	class ProcessRuntimeData {
	public:

	private:
		std::vector<byte> global_data_buffer{};
	};

}

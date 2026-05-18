#pragma once

#include "base/pointers/box.hpp"

#include <vector>

namespace vm::fast {
	namespace exec {
		struct ExecFunction;
		struct Instruction;
	}

	class ThreadRuntimeData {
	public:
		ThreadRuntimeData() = default;
		ThreadRuntimeData(const ThreadRuntimeData&)            = delete;
		ThreadRuntimeData(ThreadRuntimeData&&)                 = default;
		ThreadRuntimeData& operator=(const ThreadRuntimeData&) = delete;
		ThreadRuntimeData& operator=(ThreadRuntimeData&&)      = default;

		constexpr static usize LOCAL_STACK_SIZE = 8 * 1'024 * 1'024;  // 8 MB

	private:
		Box<std::array<byte, LOCAL_STACK_SIZE>> local_stack_memory
			= makeBox<std::array<byte, LOCAL_STACK_SIZE>>();

	public:
		std::byte* local_stack_head
			= local_stack_memory->data();  /// Pointer to the first free byte in the local stack.

		const std::byte* local_stack_base
			= local_stack_memory->data();  /// Pointer to the start of the local stack.
		const std::byte* local_stack_end
			= local_stack_memory->data()
		    + LOCAL_STACK_SIZE;  /// Pointer to the end of the local stack.
	};

	struct Frame {
		bool flag;

		const exec::Instruction* ip;

		void reset() {
			flag = false;
			ip   = nullptr;
		}
	};

	class ProcessRuntimeData {
	public:

	private:
		std::vector<byte> global_data_buffer{};
	};

}

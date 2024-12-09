#pragma once

#include <base/maps.hpp>
#include <base/unique_pointer.hpp>
#include <core/process/type_metadata/type_metadata.hpp>
#include <core/process/memory/memory_data/block.hpp>

namespace vm {
	class Memory;

	/**
	 * @brief Memory allocator
	 *
	 * Same as Allocator, but when creating a block, caller (`Executor`) has to provide
	 * pointer to the memory on the stack, that will be used to initialize the block.
	 */
	class StackAllocator {
		friend class Memory;

	private:
		Memory& memory;

		StackAllocator(Memory& process);

	public:
		BlockID makeTypeBlock(TypeCRef type, base::ModRawView data);

		BlockID makeArrayBlock(TypeCRef type, u64 length, base::ModRawView stack_ptr);

		void deleteBlock(BlockID block_id);

	};
}

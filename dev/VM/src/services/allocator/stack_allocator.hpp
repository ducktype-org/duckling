#pragma once

#include <base/maps.hpp>
#include <base/unique_pointer.hpp>
#include <services_data/type_metadata/type_metadata.hpp>

#include <memory_data/block.hpp>
#include "../services.hpp"
#include "services_data/memory/memory.hpp"

namespace vm {
	class VMProcess;

	/**
	 * @brief Memory allocator
	 *
	 * Same as Allocator, but when creating a block, caller (`Executor`) has to provide
	 * pointer to the memory on the stack, that will be used to initialize the block.
	 */
	class StackAllocator {
	private:
		Memory& memory;

		static Memory& getMemory(VMProcess& vcpu);

		template<class... DynamicServices>
		StackAllocator(ServiceManagerDef<DynamicServices...>& serviceManager):
			  memory(getMemory(serviceManager.getVCPU())) {}

	public:
		BlockID makeTypeBlock(TypeCRef type, base::ModRawView data) {
			return makeArrayBlock(type, 1, data);
		}

		BlockID makeArrayBlock(TypeCRef type, u64 length, base::ModRawView stack_ptr) {
			auto block_id = memory.reserveBlockID();
			std::memset(stack_ptr.getBegin(), 0, stack_ptr.size() * length);
			memory.makeBlock(block_id, Block(block_id, type, length, stack_ptr));
			return block_id;
		}

		void deleteBlock(BlockID block_id) {
			memory.deleteBlock(block_id);
			memory.returnBlockID(block_id);
		}

		template<class... DynamicServices>
		friend class ServiceManagerDef;
	};
}

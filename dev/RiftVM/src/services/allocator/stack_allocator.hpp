#pragma once

#include <base/maps.hpp>
#include <base/unique_pointer.hpp>
#include <services_data/type_metadata/type_metadata.hpp>

#include <memory_data/block.hpp>
#include "../services.hpp"
#include "services_data/memory/memory.hpp"

namespace vm {
	class VCPU;

	/**
	 * @brief Default dynamic memory allocator
	 */
	class StackAllocator {
		private:
			Memory& memory;

			static Memory& getMemory(VCPU& vcpu);

			template<class... DynamicServices>
			StackAllocator(ServiceManagerDef<DynamicServices...>& serviceManager): memory(getMemory(serviceManager.getVCPU())) {}
		public:
			
			BlockId makeTypeBlock(TypeCRef type, base::ModRawView data) {
				auto block_id = memory.reserveBlockID();
				memory.makeBlock(block_id, Block(block_id, type, data));
				return block_id;
			}

			BlockId makeArrayBlock(TypeCRef type, u64 length, base::ModRawView data) {
				auto block_id = memory.reserveBlockID();
				memory.makeBlock(block_id, Block(block_id, type, length, data));
				return block_id;
			}

			void deleteBlock(BlockId block_id) {
				memory.deleteBlock(block_id);
				memory.returnBlockID(block_id);
			}

			template<class... DynamicServices>
			friend class ServiceManagerDef;
	};
}
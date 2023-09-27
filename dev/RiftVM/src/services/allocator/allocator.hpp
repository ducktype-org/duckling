#pragma once

#include <base/maps.hpp>
#include <base/unique_pointer.hpp>
#include <services_data/type_metadata/type_metadata.hpp>

#include "../services.hpp"
#include "services_data/memory/memory.hpp"
#include <memory_data/block.hpp>

namespace vm {
	class VCPU;

	/**
	 * @brief Default dynamic memory allocator
	 */
	class Allocator {
	private:
		Memory &memory;

		static Memory &getMemory(VCPU &vcpu);

		template<class... DynamicServices>
		Allocator(ServiceManagerDef<DynamicServices...> &serviceManager):
			  memory(getMemory(serviceManager.getVCPU())) {}

	public:
		BlockId makeTypeBlock(TypeCRef type);

		BlockId makeArrayBlock(TypeCRef type, u64 length);

		void deleteBlock(BlockId block_id);

		template<class... DynamicServices>
		friend class ServiceManagerDef;
	};
}

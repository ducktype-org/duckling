#pragma once

#include <base/maps.hpp>
#include <base/unique_pointer.hpp>
#include <services_data/type_metadata/type_metadata.hpp>

#include <memory_data/block.hpp>
#include "services_data/memory/memory.hpp"

// This is mostly so that cmake in the linter starting from this file doesn't break in executor.hpp
namespace vm {
	class Allocator;
}

#include "../services.hpp"

namespace vm {
	class VCPU;

	/**
	 * @brief Default dynamic memory allocator
	 */
	class Allocator {
	private:
		Memory& memory;

		static Memory& getMemory(VCPU& vcpu);

		template<class... DynamicServices>
		Allocator(ServiceManagerDef<DynamicServices...>& serviceManager):
			  memory(getMemory(serviceManager.getVCPU())) {}

	public:
		BlockID makeTypeBlock(TypeCRef type);

		BlockID makeArrayBlock(TypeCRef type, u64 length);

		void deleteBlock(BlockID block_id);

		template<class... DynamicServices>
		friend class ServiceManagerDef;
	};
}

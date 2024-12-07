/**
 * @file allocator.hpp
 * @brief Dynamic memory allocator for the VM.
 *
 * Used by the Executor thread to allocate and deallocate memory.
 */
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
	class VMProcess;

	/**
	 * @brief Default dynamic memory allocator.
	 * Used by the Executor thread to allocate and deallocate memory.
	 *
	 * Uses the `Memory` data module to create and delete memory blocks.
	 * More information in the paper:
	 * ["Prototyp maszyny
	 * wirtualnej..."](https://github.com/ducktype-org/dev-space/blob/main/prace_naukowe/maszyna_wirtualna.pdf)
	 */
	class Allocator {
	private:
		Memory& memory;

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

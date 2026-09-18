#include "memory.hpp"

#include <iostream>

namespace vm {

	template<typename EntryT, typename BlockT>
	bool GenericMemory<EntryT, BlockT>::validateMemoryState() const {
#define TEST_HERE(test)                                              \
	if (test) {                                                      \
		std::cerr << #test ", BlockID=" << block.id.asInt() << "\n"; \
		return false;                                                \
	}
		for (const auto& block: blocks_pool) {
			TEST_HERE(block.refcount != 0)
			TEST_HERE(!block.deallocated)
		}
		return true;
#undef TEST_HERE
	}

	template<typename EntryT, typename BlockT>
	void GenericMemory<EntryT, BlockT>::freeAllocatedBlockData() {
		for (auto& block: blocks_pool) {
			if (!block.deallocated && !block.parent) {
				block.data.allocator->deallocate(&block.data);
				block.deallocated = true;
			}
		}
	}

	template<typename EntryT, typename BlockT>
	void GenericMemory<EntryT, BlockT>::deinitGlobals() {
		try {
			// We are first freeing all the data and then decreasing the refcounts.
			// This is very important, because there might be links between the global
			// variables, and if we were to free them and decrease the refcount in the wrong
			// order we might throw a false-positive exception. This solution avoids this
			// problem.

			for (const auto& block_ptr: global_data_blocks) freeBlockData(Ref(block_ptr));

			for (const auto& block_ptr: global_data_blocks) decreaseBlockRefcount(Ref(block_ptr));
		} catch (exceptions::VMFoundMemoryLeakException&) {
			std::cerr << "Leak during global data deinitialization - e.g. there was a global "
						 "pointer to "
						 "data, that was not freed.\n";
			throw;
		}
	}

	// Explicit instantiation for the real memory module
	template class GenericMemory<byte>;
}

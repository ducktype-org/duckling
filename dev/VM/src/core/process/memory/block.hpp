#pragma once

#include <utility>
#include <shared_mutex>
#include <base/raw_view.hpp>
#include <base/ints.hpp>
#include <base/strongly_typed_int.hpp>
#include <core/process/memory/allocator/block_data.hpp>

namespace vm {

	// @TODO: change to STRONG_TYPEDEF_IT when available
	STRONG_TYPEDEF_INT_DIMENSIONAL(BlockID, u64);

	class Block {
	private:
		BlockID                id;
		BlockData              data;
		bool                   deallocated = false;
		bool                   used        = true;
		u64                    refcount    = 0;
		Ref<std::shared_mutex> shared_mutex;
		// allocated at ...
		// freed at ...
		// name ...

		friend class Memory;

	public:
		Block(BlockID id_, BlockData data_, Ref<std::shared_mutex> mutex_):
			  id(id_),
			  data(data_),
			  shared_mutex(mutex_) {}
	};
}

#pragma once

#include <vector>
#include <base/ints.hpp>
#include <base/exceptions.hpp>
#include <memory_data/pointer.hpp>
#include <services_data/type_metadata/type.hpp>
#include <base/raw_view.hpp>
#include <base/optional.hpp>
#include <result.hpp>

namespace vm {

	class Block {
	protected:
		const u64 start = 0;
		const u64 end;

		const u64 arr_length = 0;
		TypeCRef  element_type;

		byte* data;
		// TODO: Add a way to determine which allocator created this block, as well as check if
		// appropriate allocator destroys the block.

	public:
		const BlockId block_id;

		Block(BlockId block_id_, TypeCRef type, base::ModRawView data):
			  end(type->getSize()),
			  element_type(type),
			  data(data.getBegin()),
			  block_id(block_id_) {
			RIFT_ASSERT(type->getSize() == data.size(), "type size does not equal data size");
		}

		Block(BlockId block_id_, TypeCRef type, u64 length, base::ModRawView data):
			  end(length * type->getSize()),
			  arr_length(length),
			  element_type(type),
			  data(data.getBegin()),
			  block_id(block_id_) {
			RIFT_ASSERT(
				length * type->getSize() == data.size(), "type size does not equal data size"
			);
		}

		using error = std::string;

		[[nodiscard]]
		Pointer       BasePointer() const;
		base::RawView rawPointer();

		[[nodiscard]]
		TypeCRef innerType() const;

		cpp::result<base::ModRawView, error> deref(TypeCRef u, u64 offset);
		cpp::result<base::ModRawView, error> derefCheck(TypeCRef u, u64 offset);
	};
}

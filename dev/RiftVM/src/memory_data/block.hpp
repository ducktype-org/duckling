#pragma once

#include <vector>
#include <cstdint>
#include <base/exceptions.hpp>
#include <memory_data/pointer.hpp>
#include <services_data/type_metadata/type.hpp>
#include <base/raw_view.hpp>
#include <base/option.hpp>
#include <result.hpp>

namespace vm {
	using byte = std::uint8_t;

	class Block {
	protected:
		const std::uint64_t start = 0;
		const std::uint64_t end;

		const std::uint64_t arr_length = 0;
		TypeCRef element_type;

		byte* data;
		// TODO: Add a way to determine which allocator created this block, as well as check if appropriate allocator destroys the block.
	public:
		const BlockId block_id;

		Block(BlockId block_id_, TypeCRef type, base::ModRawView data) :
				end(type->getSize()), element_type(type), data(data.getBegin()), block_id(block_id_) {
					RIFT_ASSERT(type->getSize() == data.size(), "type size does not equal data size");
				}

		Block(BlockId block_id_, TypeCRef type, std::uint64_t length, base::ModRawView data) :
				end(length * type->getSize()), 
				arr_length(length), 
				element_type(type), 
				data(data.getBegin()),
				block_id(block_id_) {
					RIFT_ASSERT(length * type->getSize() == data.size(), "type size does not equal data size");
				}

		using error = std::string;

		Pointer BasePointer() const;
		base::RawView rawPointer();

		TypeCRef innerType() const;

		result<base::ModRawView, error> deref(TypeCRef u, std::uint64_t offset);
		result<base::ModRawView, error> derefCheck(TypeCRef u, std::uint64_t offset);
	};
}
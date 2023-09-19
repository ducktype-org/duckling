/**
 * @file ctv.hpp
 * @brief defines CTV and basic CTV operations
 */

#pragma once

#include <base/ints.hpp>
#include <base/unique_pointer.hpp>
#include <span>
// @FIXME: Not including all of typesystem because templates in typesystem depend on CTVs.
#include <base/exceptions.hpp>
#include <base/named_id.hpp>
#include <typesystem/type_desc.hpp>
#include <typesystem/types.hpp>
#include <vector>

namespace exec {

	struct Block;

	// @TODO: use strongly typed int
	using BlockId = u32;

	using Data = std::vector<uint8_t>;

	std::vector<Block>& getBlocks();

	struct Block {
		Data data;

		static BlockId create(usize size) {
			getBlocks().emplace_back(Data(size, 0));

			return BlockId(getBlocks().size() - 1);
		}

		// template <typename T = uint8_t>
		// std::span<T> getData() const {
		// 	return std::span((T*)(getBlocks()[data.block].data.data() + data.offset), size/(8 *
		// sizeof(T)));
		// }

		Block(const Block&) = delete;
		Block(Block&&)      = default;

		// @TODO: this constructor should be private.
		explicit Block(Data&& data): data(std::move(data)){};
	};

	struct Pointer {
		BlockId block;
		usize   offset; /* in bits */

		// @TODO: use strongly typed ints for bit / byte offsets
		Pointer shift(usize shift /* in bits */) const {
			// @TODO: assert(offset + shift in block)

			return Pointer(block, offset + shift);
		}

		auto operator<=>(const Pointer& other) const = default;
	};

	struct CTV {
		ts::TypeDesc<> type;
		Pointer        data;
		// @TODO: it should be possible to calculate size based on type and data
		usize size;

		// @TODO: this should be more sensible but templates need it.
		// probably we should use default comparison on type here.
		auto operator<=>(const CTV&) const = default;

		CTV subCTV(ts::TypeDesc<> type_, usize offset, usize size_) const {
			return CTV(type_, data.shift(offset), size_);
		}

		template<typename T = uint8_t>
		std::span<T> getData() const {
			return std::span(
				(T*) (getBlocks()[data.block].data.data() + data.offset / ts::BYTE_SIZE),
				size / (8 * sizeof(T)));
		}

		CTV makePointer() const {
			auto pointer_type
				= ts::TypeDesc<>(ts::PointerInfo::create(type), type.getValueCategory());

			auto    block = Block::create(ts::POINTER_SIZE);
			Pointer data_(block, 0);

			CTV ctv(pointer_type, data_, ts::POINTER_SIZE);

			// @TODO: redesign how pointers are handled. In particulat when size is not 64.
			ctv.getData<u32>()[0] = data.block;
			ctv.getData<u32>()[1] = data.offset;

			// @TODO: What if ts::Pointer_size is not 64?
			// then we need to store something different in data.
			return ctv;
		}

		template<typename T = uint8_t>
		std::span<T> getDataUnderPointer() const {
			// This is only a check of whether the CTV is of type
			// Pointer or RawPointer, since both are castable to RawPointer.
			ts::RawPointerInfo(type.getType());

			auto pointer_data = getData<u32>();
			auto block_       = pointer_data[0];
			auto offset_      = pointer_data[1];

			auto& block = exec::getBlocks()[block_];

			return std::span((T*) (block.data.data() + offset_ / ts::BYTE_SIZE),
			                 block.data.size() / (8 * sizeof(T)));
		}

		CTV() = delete;

		CTV(ts::TypeDesc<> type, Pointer data, usize size): type(type), data(data), size(size) {}
	};

	CTV alloc_new(ts::TypeDesc<> type, usize size);

	inline CTV alloc_new(ts::TypeDesc<> type) {
		return alloc_new(type, type.getType().getSize());
	}
}

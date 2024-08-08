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
			DUCKLING_ASSERT(type->getSize() == data.size(), "type size does not equal data size");
		}

		Block(BlockId block_id_, TypeCRef type, u64 length, base::ModRawView data):
			  end(length * type->getSize()),
			  arr_length(length),
			  element_type(type),
			  data(data.getBegin()),
			  block_id(block_id_) {
			DUCKLING_ASSERT(
				length * type->getSize() == data.size(), "type size does not equal data size"
			);
		}

		using error = std::string;

		[[nodiscard]]
		Pointer       BasePointer() const;
		base::RawView rawPointer();

		[[gnu::always_inline]] [[nodiscard]]
		inline TypeCRef innerType() const {
			return element_type;
		}

		[[gnu::always_inline]]
		inline base::ModRawView deref(const TypeCRef& u, [[maybe_unused]] u64 offset) {
			// @NOTE: disabling these checks increases
			// load/store performance in TC by eliminating
			// 4 stack push-pops in asm
			// @NOTE: In current VM implementation offset is always 0
			// is offset actually used/will be used anywhere?
			// if (offset < start || offset > end) [[unlikely]]
			// 	DUCKLING_PANIC("Tried to defer outside of a block");
			// if (end - offset < u->getSize()) [[unlikely]]
			// 	DUCKLING_PANIC("Tried to defer too big of a type");
			return { data, u->getSize() };
		}

		cpp::result<base::ModRawView, error> derefCheck(TypeCRef u, u64 offset);
	};
}

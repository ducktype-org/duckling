#pragma once

#include <base/exceptions.hpp>
#include <services_data/type_metadata/type.hpp>
#include <vector>

namespace vm {

	// This implementation is extremely naive and inefficient
	// It is only temporary
	class TypeStack {
		std::vector<TypeCRef> types;
		Offset                byte_size;

	public:
		TypeSize         byteSize() const { return byte_size; }

		/**
		 * @brief Returns „lowest” (smallest) type at given position inside stack
		 * Used for checking if primitive of pointer type exist at position
		 */
		option<TypeCRef> lowestAtOffset(Offset offset) const {
			Offset curr_offset = 0;
			for (auto type : types) {
				if (curr_offset + type->getSize() > offset) {
					return type->getLowestTypeAtPos(offset - curr_offset);
				}
				curr_offset += type->getSize();
			}
			return none<TypeCRef>();
		}

		void push(TypeCRef type) {
			types.push_back(type);
			byte_size += type->getSize();
		}

		void pop() {
			RIFT_ASSERT(!types.empty(), "Pop called on empty stack");
			byte_size -= types.back()->getSize();
			types.pop_back();
		}
	};

}

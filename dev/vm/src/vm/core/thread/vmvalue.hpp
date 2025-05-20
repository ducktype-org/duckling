#pragma once

#include <base/raw_view.hpp>

#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>

#include <iostream>

namespace vm {
	/**
	 * @brief Storage for a value. It is meant to import value into/export value out of VM.
	 * It is NOT meant to be used by the internal memory module.
	 * @note Passed data is copied.
	 * @note User of VmValue is responsible for cleaning it up.
	 */
	struct VmValue {
	private:
		std::vector<byte> data;  // data.size() == type.getSize()
		Ref<Memory>       memory;

	public:
		VmValue(const VmValue&)            = delete;
		VmValue(VmValue&&)                 = default;
		VmValue& operator=(const VmValue&) = delete;
		VmValue& operator=(VmValue&&)      = default;

		explicit VmValue(TypeCRef type, Memory& memory):
			  data(type->getSize()),
			  memory(&memory),
			  type(type),
			  pointer(Memory::getPointer(memory.allocateStack(type, data.data()))) {}

		VmValue(TypeCRef type, Memory& memory, Pointer src): VmValue(type, memory) {
			importData(src);
		}

		~VmValue() {
			if (!pointer.isNull()) std::cerr << "VmValue not freed!\n";
		}

		void exportData(Pointer dst) { memory->copyPointerData(dst, pointer, type); }

		void importData(Pointer src) { memory->copyPointerData(pointer, src, type); }

		void freeData() {
			memory->freeBlock(pointer.getBlock());
			pointer = Pointer::null();
		}

		TypeCRef type;
		Pointer  pointer;

		template<class T>
		constexpr T& interpret(usize offset = 0) {
			return *reinterpret_cast<T*>(data.data() + offset);
		}

		template<class T>
		constexpr const T& interpret(usize offset = 0) const {
			return *reinterpret_cast<const T*>(data.data() + offset);
		}
	};
}

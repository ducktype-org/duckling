#pragma once

#include <base/raw_view.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>

namespace vm {
	/**
	 * @brief Storage for a value. It is meant to import value into/export value out of VM.
	 * It is NOT meant to be used by the internal memory module.
	 * @note Passed data is copied.
	 */
	struct VmValue {
		explicit VmValue(TypeCRef type): type(type) { data.resize(type->getSize()); }

		VmValue(TypeCRef type, const byte* data): VmValue(type) { setValue(data); }

		/**
		 * @brief Sets value's data as new_data. Assumes new_data.size() >= type.getSize();
		 */
		void setValue(const byte* new_data) {
			CORE_ASSERT(new_data != nullptr, "VmValue\'s data cannot be null!");
			std::memcpy(this->data.data(), new_data, type->getSize());
		}

		std::vector<byte> data;  // data.size() == type.getSize()
		TypeCRef          type;

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

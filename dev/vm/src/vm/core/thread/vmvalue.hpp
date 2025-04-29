#pragma once

#include <base/raw_view.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>

namespace vm {
	/**
	 * @brief Storage for a value. It is meant to import value into/export value out of VM.
	 * @note It is not meant to be used by the internal memory module.
	 */
	struct VmValue {
		VmValue(TypeCRef type): type(type) { data.resize(type->getSize()); }

		VmValue(TypeCRef type, const byte* data): VmValue(type) { setValue(data); }

		/**
		 * @brief Sets value's data as new_data. Assumes new_data.size() >= type.getSize();
		 */
		void setValue(const byte* new_data) {
			std::memcpy(this->data.data(), new_data, type->getSize());
		}

		std::vector<byte> data;  // data.size() == type.getSize()
		TypeCRef          type;

		template<class T>
		T& intepret(usize offset = 0) {
			return *reinterpret_cast<T*>(data.data() + offset);
		}

		template<class T>
		const T& intepret(usize offset = 0) const {
			return *reinterpret_cast<const T*>(data.data() + offset);
		}
	};
}

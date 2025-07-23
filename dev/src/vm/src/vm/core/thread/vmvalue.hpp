#pragma once

#include <base/raw_view.hpp>

#include <vm/api/data/process_info.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/utils/interpret.hpp>

#include <iostream>

namespace vm {
	class VMProcess;

	/**
	 * @brief Storage for a value. It is meant to import value into/export value out of VM.
	 * It is NOT meant to be used by the internal memory module.
	 * @note Passed data is copied.
	 * @note User of VmValue is responsible for releasing the held blocks.
	 */
	class VmValue {
		std::vector<byte> data;        /// data.size() == type.getSize()
		Ref<VMProcess>    my_process;  /// The process for which the VmValue exists.
		Ref<Memory>       memory;

	public:
		VmValue(const VmValue&)            = delete;
		VmValue(VmValue&&)                 = default;
		VmValue& operator=(const VmValue&) = delete;
		VmValue& operator=(VmValue&&)      = default;

		VmValue(VMProcess& process, TypeCRef type);

		VmValue(VMProcess& process, TypeCRef type, Pointer src);

		~VmValue();

		void exportData(Pointer dst) const;

		void importData(Pointer src);

		void freeData();

		PID getPID() const;

		TypeCRef type;
		Pointer  pointer;

		template<class T>
		[[nodiscard]]
		constexpr T& interpret(const usize offset = 0) {
			// @TODO: Fix
			// return interpretBytes<T>(*(data.data() + offset));
			return *reinterpret_cast<T*>(data.data() + offset);
		}

		template<class T>
		[[nodiscard]] constexpr const T& interpret(const usize offset = 0) const {
			// @TODO: Fix
			// return interpretBytes<const T>(*(data.data() + offset));
			return *reinterpret_cast<const T*>(data.data() + offset);
		}

		[[nodiscard]] byte* getBytes();

		[[nodiscard]] const byte* getBytes() const;
	};
}

JSON_REGISTER_TYPE_WITH_NAME(vm::VmValue, "VmValue");

template<>
struct nlohmann::adl_serializer<vm::VmValue> {
	static void to_json(json& j, const vm::VmValue& v) {
		j["type"]        = std::string(TypeParseTraits<vm::VmValue>::name.data());
		j["data_type"]   = v.type->getName().str();
		j["data_length"] = v.type->getSize();
		// Convert VmValue's bytes to HEX string
		// @TODO: Test this
		std::stringstream ss;
		ss << std::hex;
		for (size_t i = 0; i < v.type->getSize(); ++i)
			ss << std::setw(2) << std::setfill('0') << static_cast<int>(v.getBytes()[i]);
		j["data"] = ss.str();
	}

	static void from_json(const json&, const vm::VmValue&) {
		CORE_PANIC("Parsing data from JSON into a VmValue is not supported (yet).");
	}
};

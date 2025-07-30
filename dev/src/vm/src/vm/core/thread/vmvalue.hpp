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
	 * @note VmValue is owned by the process which created it. It can only be created with the
	 `VMProcess::createVmValue()` function. It's automatically freed when VMProcess is freed.
	 */
	class VmValue {
	private:
		friend class VMProcess;
		VmValue(VMProcess& process, TypeCRef type);
		VmValue(VMProcess& process, TypeCRef type, Pointer src);
		void freeData();

		std::vector<byte> data;        /// data.size() == type.getSize()
		Ref<VMProcess>    my_process;  /// The process for which the VmValue exists.
		Ref<Memory>       memory;

	public:
		VmValue(const VmValue&)            = delete;
		VmValue(VmValue&&)                 = default;
		VmValue& operator=(const VmValue&) = delete;
		VmValue& operator=(VmValue&&)      = default;


		~VmValue();

		void exportData(Pointer dst) const;

		void importData(Pointer src);


		[[nodiscard]] PID getPID() const;

		TypeCRef type;
		Pointer  pointer;

		/**
		 * @brief Interprets a constant raw byte buffer pointed to by `ptr` as an object of type T.
		 * @warning This operation is unsafe and violates strict alignment rules.
		 * The caller is expected to know what they are doing. Incorrect usage may lead to UB.
		 *
		 * @todo: This is unsafe. Maybe there's a better way. This should change in #1133.
		 **/
		template<class T>
		constexpr T& interpret(const usize offset = 0) {
			CORE_ASSERT(
				type->getName() != base::StrID("void"), "Interpreting VmValue bytes of type void!"
			);
			CORE_ASSERT(
				offset + sizeof(T) <= data.size(), "Interpreting as value exceeding the data.size()"
			);
			return interpretBytes<T>(data.data() + offset);
		}

		/**
		 * @brief Interprets a constant raw byte buffer pointed to by `ptr` as an object of type T.
		 * @warning This operation is unsafe and violates strict alignment rules.
		 * The caller is expected to know what they are doing. Incorrect usage may lead to UB.
		 *
		 * @todo: This is unsafe. Maybe there's a better way. This should change in #1133.
		 **/
		template<class T>
		[[nodiscard]] constexpr const T& interpret(const usize offset = 0) const {
			CORE_ASSERT(
				type->getName() != base::StrID("void"), "Interpreting VmValue bytes of type void!"
			);
			CORE_ASSERT(
				offset + sizeof(T) <= data.size(), "Interpreting as value exceeding the data.size()"
			);
			return interpretBytes<const T>(data.data() + offset);
		}

		[[nodiscard]] byte* getBytes();

		[[nodiscard]] const byte* getBytes() const;
	};
}

JSON_REGISTER_TYPE_WITH_NAME(vm::VmValue, "VmValue");

// NOLINTBEGIN(readability-identifier-naming)
template<>
struct nlohmann::adl_serializer<vm::VmValue> {
	static void to_json(json& j, const vm::VmValue& v) {
		j["type"]        = std::string(TypeParseTraits<vm::VmValue>::name.data());
		j["data_type"]   = v.type->getName().str();
		j["data_length"] = v.type->getSize();
		// Convert VmValue's bytes to HEX string
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

// NOLINTEND(readability-identifier-naming)

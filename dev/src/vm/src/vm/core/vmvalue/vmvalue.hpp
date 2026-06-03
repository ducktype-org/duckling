#pragma once


#include <vm/api/data/process_info.hpp>
#include <vm/core/safe/memory/memory.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/vmvalue/vmvalueref.hpp>
#include <vm/utils/interpret.hpp>

#include <ostream>

namespace vm {
	class SafeVMProcess;

	/**
	 * @brief Storage for a value. It is meant to import value into/export value out of VM.
	 * It is NOT meant to be used by the internal memory module.
	 * @note Passed data is copied.
	 * @note VmValues can be used only in the processes which where used when initializing them.
	 * They can't be transferred in between different processes.
	 *
	 * @note There are two ways to create a VmValue:
	 * 1) with `VMProcess::createVmValue()` function - creates a VmValue owned by the process. Its
	 * lifetime is guarded by VMProcess. All VmValues created by this function are deinitialized
	 * when VmProcess is destroyed.
	 * 2) with `VMProcess::createOwnedVmValue()` function - creates a
	 * VmValue and transfers the ownership to the caller. The caller is expected to free the VmValue
	 * by calling freeData().
	 */
	class VmValue final {
	private:
		friend class SafeVMProcess;

		/**
		 * @brief Creates an empty VmValue of the specified type.
		 */
		VmValue(SafeVMProcess& process, TypeCRef type);

		/**
		 * @brief Creates a VmValue of specified type and fills it with the bytes from the `src`
		 * pointer.
		 */
		VmValue(SafeVMProcess& process, TypeCRef type, Pointer src);

		std::vector<byte>  data;        /// data.size() == type.getSize()
		Ref<SafeVMProcess> my_process;  /// The process for which the VmValue exists.
		Ref<Memory>        memory;

	public:
		VmValue(const VmValue&)            = delete;
		VmValue(VmValue&&)                 = default;
		VmValue& operator=(const VmValue&) = delete;
		VmValue& operator=(VmValue&&)      = default;

		~VmValue();

		/**
		 * @brief Frees the data of the VmValue (deinitializes the blocks in the memory module).
		 * This function has to be called when using VmValues created with the
		 * `VMProcess::createOwnedVmValue()` function.
		 *
		 * @note At first glance, one could wonder why do you have to manually call freeData()
		 * instead of putting the free'ing logic into the vmvalue destructor. The answer is -
		 * freeing blocks in the memory module isn't exception free.
		 */
		void freeData();

		void exportData(Pointer dst) const;

		void importData(Pointer src);

		[[nodiscard]] VMValueRef asRef() const;

		[[nodiscard]] base::CRef<code::valid_type::ValidType> getType() const;

		[[nodiscard]] base::Optional<InterpretedDataVariant> readData() const;

		[[nodiscard]] PID getPID() const;

		TypeCRef type;
		Pointer  pointer;

		/**
		 * @brief Interprets a constant raw byte buffer pointed to by `ptr` as an object of type T.
		 */
		template<class T>
		T readBytes() const {
			CORE_ASSERT(sizeof(T) <= data.size(), "VmValue: Out of bounds read");
			return vm::safeReadPointerBytes<T>(data.data());
		}

		/**
		 * @brief Interprets a constant raw byte buffer pointed to by `ptr` as an object of type T.
		 */
		template<class T>
		void writeBytes(const T& value, const usize offset = 0) {
			CORE_ASSERT(offset + sizeof(T) <= data.size(), "VmValue: Out of bounds write");
			return vm::safeWriteBytes<T>(data.data(), value);
		}

		[[nodiscard]] byte* getBytes();

		[[nodiscard]] const byte* getBytes() const;

		/**
		 * @brief Prints a detailed, human-readable representation of the VmValue.
		 * Attempts to interpret the value's bytes based on its type and additionally prints the hex
		 * dump.
		 * @param out The output stream to print to.
		 * @param indent A prefix string for indentation.
		 */
		void dprint(std::ostream& out, const std::string& indent = "") const;
	};
}

JSON_REGISTER_TYPE_WITH_NAME(vm::VmValue, "VmValue");
JSON_REGISTER_TYPE_WITH_NAME(Ref<vm::VmValue>, "Ref<VmValue>");

// NOLINTBEGIN(readability-identifier-naming)
template<>
struct nlohmann::adl_serializer<vm::VmValue> {
	static void to_json(json& j, const vm::VmValue& v) {
		j["type"]        = std::string(TypeParseTraits<vm::VmValue>::NAME.data());
		j["data_type"]   = v.type->getName().str();
		j["data_length"] = v.type->getSize();
		// Convert VmValue's bytes to HEX string
		std::stringstream ss;
		ss << std::hex;
		for (size_t i = 0; i < v.type->getSize().asInt(); ++i)
			ss << std::setw(2) << std::setfill('0') << static_cast<int>(v.getBytes()[i]);
		j["data"] = ss.str();
	}

	static void from_json(const json&, const vm::VmValue&) {
		CORE_PANIC("Parsing data from JSON into a VmValue is not supported (yet).");
	}
};

// NOLINTEND(readability-identifier-naming)

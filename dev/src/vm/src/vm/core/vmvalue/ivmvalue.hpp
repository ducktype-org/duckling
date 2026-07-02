#pragma once

#include <base/collections/optional.hpp>

#include <vm/api/data/process_info.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/core/safe/memory/pointer.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/vmvalue/ivmvalueref.hpp>
#include <vm/utils/interpret.hpp>

#include <ostream>

namespace vm {
	/**
	 * @brief Common interface for a value stored outside the VM. It is meant to import a value
	 * into / export a value out of the VM. It is NOT meant to be used by the internal memory module.
	 * @note VmValues can be used only in the processes which were used when initializing them.
	 * They can't be transferred in between different processes.
	 */
	class IVmValue {
	public:
		virtual ~IVmValue() = default;

		TypeCRef type;

		/**
		 * @brief Frees the data of the value (deinitializes the blocks in the memory module).
		 * This function has to be called when using values created with the
		 * `VMProcess::createOwnedVmValue()` function.
		 */
		virtual void freeData() = 0;

		/** @brief Copies the value's data into the memory pointed to by `dst`. */
		virtual void exportData(Pointer dst) const = 0;

		/** @brief Fills the value's data with the bytes pointed to by `src`. */
		virtual void importData(Pointer src) = 0;

		/** @brief Returns the high-level (compiler) type of the value. */
		[[nodiscard]] virtual base::CRef<code::valid_type::ValidType> getType() const = 0;

		/** @brief Interprets the value's data as a structured, human-inspectable variant. */
		[[nodiscard]] virtual base::Optional<InterpretedDataVariant> readData() const = 0;

		/** @brief Returns the PID of the process this value belongs to. */
		[[nodiscard]] virtual PID getPID() const = 0;

		/** @brief Returns a mutable pointer to the value's raw byte buffer. */
		[[nodiscard]] virtual byte* getBytes() = 0;

		/** @brief Returns a constant pointer to the value's raw byte buffer. */
		[[nodiscard]] virtual const byte* getBytes() const = 0;

		/**
		 * @brief Prints a detailed, human-readable representation of the value.
		 * Attempts to interpret the value's bytes based on its type and additionally prints the hex
		 * dump.
		 */
		virtual void dprint(std::ostream& out, const std::string& indent = "") const = 0;

		/**
		 * @brief Interprets the value's raw byte buffer as an object of type T.
		 */
		template<class T>
		T readBytes() const {
			CORE_ASSERT(sizeof(T) <= static_cast<usize>(type->getSize()), "VmValue: Out of bounds read");
			return vm::safeReadPointerBytes<T>(getBytes());
		}

		/**
		 * @brief Writes the byte representation of `value` into the value's raw byte buffer.
		 */
		template<class T>
		void writeBytes(const T& value, const usize offset = 0) {
			CORE_ASSERT(
				offset + sizeof(T) <= static_cast<usize>(type->getSize()), "VmValue: Out of bounds write"
			);
			return vm::safeWriteBytes<T>(getBytes(), value);
		}

	protected:
		explicit IVmValue(TypeCRef type): type(type) {}
	};
}

JSON_REGISTER_TYPE_WITH_NAME(vm::IVmValue, "VmValue");
JSON_REGISTER_TYPE_WITH_NAME(Ref<vm::IVmValue>, "Ref<VmValue>");

// NOLINTBEGIN(readability-identifier-naming)
template<>
struct nlohmann::adl_serializer<vm::IVmValue> {
	static void to_json(json& j, const vm::IVmValue& v) {
		j["type"]        = std::string(TypeParseTraits<vm::IVmValue>::NAME.data());
		j["data_type"]   = v.type->getName().str();
		j["data_length"] = v.type->getSize();
		// Convert value's bytes to HEX string
		std::stringstream ss;
		ss << std::hex;
		for (size_t i = 0; i < v.type->getSize().asInt(); ++i)
			ss << std::setw(2) << std::setfill('0') << static_cast<int>(v.getBytes()[i]);
		j["data"] = ss.str();
	}

	static void from_json(const json&, const vm::IVmValue&) {
		CORE_PANIC("Parsing data from JSON into a VmValue is not supported (yet).");
	}
};

// NOLINTEND(readability-identifier-naming)

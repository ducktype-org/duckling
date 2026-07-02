#pragma once

#include <vm/api/data/process_info.hpp>
#include <vm/core/safe/memory/memory.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalueref.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>
#include <vm/utils/interpret.hpp>

#include <ostream>

namespace vm {
	class SafeVMProcess;

	/**
	 * @brief Storage for a value in the safe VM. It is meant to import a value into / export a value
	 * out of the VM. It is NOT meant to be used by the internal memory module.
	 * @note Passed data is copied.
	 * @note SafeVmValues can be used only in the processes which were used when initializing them.
	 * They can't be transferred in between different processes.
	 *
	 * @note There are two ways to create a SafeVmValue:
	 * 1) with `VMProcess::createVmValue()` function - creates a value owned by the process. Its
	 * lifetime is guarded by VMProcess. All values created by this function are deinitialized
	 * when VMProcess is destroyed.
	 * 2) with `VMProcess::createOwnedVmValue()` function - creates a value and transfers the
	 * ownership to the caller. The caller is expected to free the value by calling freeData().
	 */
	class SafeVmValue final: public IVmValue {
	private:
		friend class SafeVMProcess;

		/**
		 * @brief Creates an empty SafeVmValue of the specified type.
		 */
		SafeVmValue(SafeVMProcess& process, TypeCRef type);

		/**
		 * @brief Creates a SafeVmValue of specified type and fills it with the bytes from the `src`
		 * pointer.
		 */
		SafeVmValue(SafeVMProcess& process, TypeCRef type, Pointer src);

		std::vector<byte>  data;        /// data.size() == type.getSize()
		Ref<SafeVMProcess> my_process;  /// The process for which the SafeVmValue exists.
		Ref<Memory>        memory;

	public:
		SafeVmValue(const SafeVmValue&)            = delete;
		SafeVmValue(SafeVmValue&&)                 = default;
		SafeVmValue& operator=(const SafeVmValue&) = delete;
		SafeVmValue& operator=(SafeVmValue&&)      = default;

		~SafeVmValue() override;

		/**
		 * @brief Frees the data of the SafeVmValue (deinitializes the blocks in the memory module).
		 * This function has to be called when using values created with the
		 * `VMProcess::createOwnedVmValue()` function.
		 *
		 * @note At first glance, one could wonder why do you have to manually call freeData()
		 * instead of putting the free'ing logic into the destructor. The answer is - freeing blocks
		 * in the memory module isn't exception free.
		 */
		void freeData() override;

		void exportData(Pointer dst) const override;

		void importData(Pointer src) override;

		[[nodiscard]] SafeVmValueRef asRef() const;

		[[nodiscard]] base::CRef<code::valid_type::ValidType> getType() const override;

		[[nodiscard]] base::Optional<InterpretedDataVariant> readData() const override;

		[[nodiscard]] PID getPID() const override;

		Pointer pointer;

		[[nodiscard]] byte* getBytes() override;

		[[nodiscard]] const byte* getBytes() const override;

		/**
		 * @brief Prints a detailed, human-readable representation of the SafeVmValue.
		 * Attempts to interpret the value's bytes based on its type and additionally prints the hex
		 * dump.
		 * @param out The output stream to print to.
		 * @param indent A prefix string for indentation.
		 */
		void dprint(std::ostream& out, const std::string& indent = "") const override;
	};
}

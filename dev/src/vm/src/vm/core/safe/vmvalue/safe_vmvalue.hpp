#pragma once

#include <vm/api/data/process_info.hpp>
#include <vm/core/safe/memory/memory.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalueref.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>
#include <vm/utils/interpret.hpp>

#include <limits>
#include <ostream>

namespace vm {
	class SafeVMProcess;

	/**
	 * @brief Storage for a value in the safe VM. It is meant to import a value into / export a
	 * value out of the VM. It is NOT meant to be used by the internal memory module.
	 * @note Passed data is copied.
	 * @note SafeVMValues can be used only in the processes which were used when initializing them.
	 * They can't be transferred in between different processes.
	 *
	 * @note There are two ways to create a SafeVMValue:
	 * 1) with `SafeVMProcess::createVMValue()` - the value is owned by the process (kept alive in
	 * its list of owned values) and its data is freed automatically when the process is
	 * deinitialized. 2) with `SafeVMProcess::createOwnedVMValue()` - ownership is transferred to
	 * the caller, who is responsible for freeing the data by calling `freeData()` (the destructor
	 * does not free it).
	 */
	class SafeVMValue final: public IVMValue {
	private:
		friend class SafeVMProcess;

		/**
		 * @brief Creates an empty SafeVMValue of the specified type.
		 */
		SafeVMValue(SafeVMProcess& process, TypeCRef type);

		/**
		 * @brief Creates a SafeVMValue of specified type and fills it with the bytes from the `src`
		 * pointer.
		 */
		SafeVMValue(SafeVMProcess& process, TypeCRef type, Pointer src);

		std::vector<byte>  data;        /// data.size() == type.getSize()
		Ref<SafeVMProcess> my_process;  /// The process for which the SafeVMValue exists.
		Ref<Memory>        memory;
		u64                id;

		static constexpr u64 UNREGISTERED_ID = std::numeric_limits<u64>::max();

	public:
		SafeVMValue(const SafeVMValue&)            = delete;
		SafeVMValue(SafeVMValue&&)                 = default;
		SafeVMValue& operator=(const SafeVMValue&) = delete;
		SafeVMValue& operator=(SafeVMValue&&)      = default;

		~SafeVMValue() override;

		/**
		 * @brief Frees the data of the SafeVMValue (deinitializes the blocks in the memory module).
		 * This function has to be called manually when using values created with the
		 * `SafeVMProcess::createOwnedVMValue()` function (process-owned values are freed for you).
		 *
		 * @note At first glance, one could wonder why do you have to manually call freeData()
		 * instead of putting the free'ing logic into the destructor. The answer is - freeing blocks
		 * in the memory module isn't exception free.
		 */
		void freeData() override;

		/** @brief Copies the value's data into the safe VM memory pointed to by `dst`. */
		void exportData(Pointer dst) const;

		/** @brief Fills the value's data with the bytes pointed to by `src`. */
		void importData(Pointer src);

		/** @copydoc IVMValue::importDataFrom */
		void importDataFrom(const IVMValue& source) override;

		[[nodiscard]] SafeVMValueRef asRef() const;

		[[nodiscard]] base::CRef<code::valid_type::ValidType> getType() const override;

		[[nodiscard]] code::valid_type::ValidTypeID getTypeID() const override;

		[[nodiscard]] Bytes getDataSize() const override;

		[[nodiscard]] base::Optional<InterpretedDataVariant> readData() const override;

		[[nodiscard]] PID getPID() const override;

		[[nodiscard]] u64 getValueID() const;

		/// Safe VM runtime type metadata of the stored value.
		TypeCRef type;

	private:
		Pointer pointer;

	public:
		[[nodiscard]] byte* getBytes() override;

		[[nodiscard]] const byte* getBytes() const override;

		/** @copydoc IVMValue::dprint */
		void dprint(std::ostream& out, const std::string& indent = "") const override;
	};
}

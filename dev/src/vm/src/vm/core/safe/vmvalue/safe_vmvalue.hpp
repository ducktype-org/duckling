// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <vm/api/data/process_info.hpp>
#include <vm/core/safe/memory/frame.hpp>
#include <vm/core/safe/memory/memory.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalueref.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>
#include <vm/utils/interpret.hpp>

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
	 * the caller, who is responsible for freeing the data (the destructor does not free it).
	 * Outside of the VM that is done with `api::freeVMValue`.
	 *
	 * @note A value keeps its bytes inside itself and gets a memory block only when it is filled
	 * from data that has child blocks (e.g. a variant's payload view). Reading a value never
	 * creates a block, see `SafeDataLocation`.
	 */
	class SafeVMValue final: public IVMValue {
	private:
		friend class SafeVMProcess;
		// @note: This is because we call freeData() in `call_builtinfunc` opcode implementation.
		friend class OpFuns;

		/**
		 * @brief Creates a zeroed SafeVMValue of the specified type.
		 */
		SafeVMValue(SafeVMProcess& process, TypeCRef type);

		SharedBox<SafeVMValueBytes> value_bytes;
		Ref<SafeVMProcess>          my_process;  /// The process for which the SafeVMValue exists.
		Ref<Memory>                 memory;

		/// Null until the data needs a block, see the class note.
		Block* block = nullptr;

		/// True when the VMValue is owned by the process (i.e. function return values). The process
		/// frees them on it's own.
		bool owned_by_process = false;

		[[nodiscard]] byte* storage() const { return value_bytes->data(); }

		[[nodiscard]] base::TypedModRawView<byte> bytesView() const {
			return { storage(), type->getSize().asInt() };
		}

		/// @throws exceptions::VMUseAfterFreeException if the value is freed.
		void checkNotFreed() const;

		/// The value's block, created over its bytes when it has none yet.
		[[nodiscard]] Pointer ensurePointer();

		/// Replaces the value with plain bytes that have no child blocks.
		void importBytes(const byte* src);

		/// Replaces the value with the data of `src_block` when there is one, else `src_bytes`.
		void importFrom(Block* src_block, const byte* src_bytes);

		/**
		 * @brief Frees the data of the SafeVMValue: runs its destructors and releases its block, if
		 * it has one.
		 * This function has to be called manually when using values created with the
		 * `SafeVMProcess::createOwnedVMValue()` function (process-owned values are freed for you).
		 *
		 * @note At first glance, one could wonder why do you have to manually call freeData()
		 * instead of putting the free'ing logic into the destructor. The answer is - freeing blocks
		 * in the memory module isn't exception free.
		 *
		 * @note Private, because it is not synchronized with the process. Only the VM internals
		 * can call it directly, everyone else frees a value with `api::freeVMValue`.
		 */
		void freeData();

	public:
		// Always boxed, nothing needs to move one.
		SafeVMValue(const SafeVMValue&)            = delete;
		SafeVMValue(SafeVMValue&&)                 = delete;
		SafeVMValue& operator=(const SafeVMValue&) = delete;
		SafeVMValue& operator=(SafeVMValue&&)      = delete;

		~SafeVMValue() override;

		/** @brief Copies the value's data into the safe VM memory pointed to by `dst`. */
		void exportData(Pointer dst) const;

		/** @brief Fills the value's data with the bytes pointed to by `src`. */
		void importData(Pointer src);

		/**
		 * @brief Copies the value into a local variable. A variable without a block gets none,
		 * unless the value holds child blocks.
		 */
		void exportToSlot(LocalSlot& slot) const;

		/**
		 * @brief Fills the value from a local variable, without creating a block for the
		 * variable.
		 */
		void importFromSlot(const LocalSlot& slot);

		[[nodiscard]] bool isFreed() const { return value_bytes->freed; }

		/// Whether the value has a memory block yet, see the class note.
		[[nodiscard]] bool hasBlock() const { return block != nullptr; }

		/** @copydoc IVMValue::importDataFrom */
		void importDataFrom(const IVMValue& source) override;

		[[nodiscard]] SafeVMValueRef asRef() const;

		[[nodiscard]] base::CRef<code::valid_type::ValidType> getType() const override;

		[[nodiscard]] code::valid_type::ValidTypeID getTypeID() const override;

		[[nodiscard]] Bytes getDataSize() const override;

		[[nodiscard]] base::Optional<InterpretedDataVariant> readData() const override;

		[[nodiscard]] PID getPID() const override;

		/// Safe VM runtime type metadata of the stored value.
		TypeCRef type;

		[[nodiscard]] byte* getBytes() override;

		[[nodiscard]] const byte* getBytes() const override;

		/** @copydoc IVMValue::dprint */
		void dprint(std::ostream& out, const std::string& indent = "") const override;
	};
}

// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/optional.hpp>
#include <base/pointers/shared_box.hpp>

#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/core/safe/memory/memory.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/vmvalue/ivmvalueref.hpp>
#include <vm/utils/interpret.hpp>

#include <array>
#include <cstddef>
#include <variant>
#include <vector>

namespace vm {
	class SafeVMProcess;

	/**
	 * @brief The bytes of a `SafeVMValue`. The value shares them with its references, so a
	 * reference that outlives the value reports a use after free instead of dangling.
	 */
	class SafeVMValueBytes final {
	private:
		/// Values up to this size keep their bytes inline, bigger ones on the heap.
		static constexpr usize INLINE_SIZE = 16;

		alignas(std::max_align_t) std::array<byte, INLINE_SIZE> inline_bytes{};
		std::vector<byte> heap_bytes;  /// Empty when the bytes are inline.
		usize             size;

	public:
		bool freed = false;

		/// Zeroed bytes.
		explicit SafeVMValueBytes(usize size);

		[[nodiscard]] byte* data() {
			return heap_bytes.empty() ? inline_bytes.data() : heap_bytes.data();
		}

		[[nodiscard]] usize getSize() const { return size; }

		/**
		 * @brief The bytes from `offset` on.
		 * @throws exceptions::VMUseAfterFreeException if the value is freed.
		 * @throws exceptions::VMOutOfBlockBoundsException if `offset` is past the end.
		 */
		[[nodiscard]] base::TypedModRawView<byte> from(u64 offset);
	};

	/**
	 * @brief Data of a `SafeVMValue` that has no memory block: its own bytes, from `offset` on.
	 */
	struct ValueBytesLocation final {
		SharedBox<SafeVMValueBytes> bytes;
		u64                         offset = 0;
	};

	/// Where referenced data lives: in a memory block, or in the bytes of a `SafeVMValue`.
	using SafeDataLocation = std::variant<Pointer, ValueBytesLocation>;

	/// `location` moved forward by `bytes`.
	[[nodiscard]] SafeDataLocation movedLocation(const SafeDataLocation& location, u64 bytes);

	/**
	 * @brief Safe VM implementation of `IVMValueRef`. References a value living in the safe VM's
	 * memory module or in a `SafeVMValue`; it is NOT meant to be used by the internal memory
	 * module itself.
	 */
	class SafeVMValueRef final: public IVMValueRef {
	private:
		const Ref<SafeVMProcess> my_process;
		const Ref<Memory>        memory;
		const TypeCRef           my_type;
		const SafeDataLocation   location;

		/// The `size` bytes the reference starts at, checked for null and use after free.
		[[nodiscard]] base::TypedModRawView<byte> dataView(usize size) const;

	public:
		SafeVMValueRef(SafeVMProcess& process, TypeCRef type, SafeDataLocation location);

		/**
		 * @brief Creates a shared, interface-typed reference to safe VM data.
		 * Single place funneling the allocation of `SafeVMValueRef`s into a `SharedBox`.
		 */
		[[nodiscard]] static SharedBox<IVMValueRef> makeShared(
			SafeVMProcess& process, TypeCRef type, SafeDataLocation location
		);

		[[nodiscard]] base::CRef<code::valid_type::ValidType> getType() const override;
		[[nodiscard]] base::Optional<InterpretedDataVariant>  readData() const override;
		[[nodiscard]] std::string                             str() const override;
		[[nodiscard]] bool                                    isComplex() const override;

		template<class T>
		requires std::is_trivially_copy_constructible_v<T> T readBytes() const {
			CORE_ASSERT(my_type->getName() != "void", "Interpreting VMValueRef bytes of type void!");
			CORE_ASSERT(
				sizeof(T) <= static_cast<usize>(my_type->getSize()), "VMValueRef: Out of bounds read"
			);

			return vm::safeReadPointerBytes<T>(dataView(sizeof(T)).getBegin());
		}
	};

	/**
	 * @brief Element access to a table stored in the safe VM's memory.
	 * Elements are resolved lazily - a `SafeVMValueRef` is created per requested index.
	 */
	class SafeTableElementAccess final: public ITableElementAccess {
	private:
		const Ref<SafeVMProcess> my_process;
		const TypeCRef           element_type;
		const SafeDataLocation   begin;

	public:
		SafeTableElementAccess(SafeVMProcess& process, TypeCRef element_type, SafeDataLocation begin);

		[[nodiscard]] SharedBox<IVMValueRef> get(usize index) const override;

		/** @copydoc ITableElementAccess::asBytesView */
		[[nodiscard]] base::ModRawView asBytesView() const override;
	};
}

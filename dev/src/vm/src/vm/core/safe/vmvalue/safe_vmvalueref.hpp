#pragma once

#include <base/collections/optional.hpp>

#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/core/safe/memory/memory.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/vmvalue/ivmvalueref.hpp>
#include <vm/utils/interpret.hpp>

namespace vm {
	class SafeVMProcess;

	/**
	 * @brief Safe VM implementation of `IVMValueRef`. References a value living in the safe VM's
	 * memory module; it is NOT meant to be used by the internal memory module itself.
	 */
	class SafeVMValueRef final: public IVMValueRef {
	private:
		const Ref<SafeVMProcess> my_process;
		const Ref<Memory>        memory;
		const TypeCRef           my_type;
		const Pointer            pointed_data;

	public:
		SafeVMValueRef(SafeVMProcess& process, TypeCRef type, Pointer pointed_data);

		/**
		 * @brief Creates a shared, interface-typed reference to safe VM data.
		 * Single place funneling the allocation of `SafeVMValueRef`s into a `SharedBox`.
		 */
		[[nodiscard]] static SharedBox<IVMValueRef> makeShared(
			SafeVMProcess& process, TypeCRef type, Pointer pointed_data
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

			auto view = memory->getPointerData(pointed_data, sizeof(T));
			return vm::safeReadPointerBytes<T>(view.getBegin());
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
		const Pointer            begin;

	public:
		SafeTableElementAccess(SafeVMProcess& process, TypeCRef element_type, Pointer begin);

		[[nodiscard]] SharedBox<IVMValueRef> get(usize index) const override;

		/** @copydoc ITableElementAccess::asBytesView */
		[[nodiscard]] base::ModRawView asBytesView() const override;
	};
}

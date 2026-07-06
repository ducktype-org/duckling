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
	 * @brief Reference for a value in the safe VM. It is meant to give access to the value from
	 * outside the VM. It is NOT meant to be used by the internal memory module.
	 * @note Passed data is NOT copied - only referenced.
	 * @note `SafeVmValueRef`s can only be used within the same process where they were initialized.
	 * They cannot be transferred to different processes.
	 */
	class SafeVmValueRef final: public IVmValueRef {
	private:
		const Ref<SafeVMProcess> my_process;
		const Ref<Memory>        memory;
		const TypeCRef           my_type;
		const Pointer            pointed_data;

	public:
		SafeVmValueRef(SafeVMProcess& process, TypeCRef type, Pointer pointed_data);

		[[nodiscard]] base::CRef<code::valid_type::ValidType> getType() const override;
		[[nodiscard]] base::Optional<InterpretedDataVariant>  readData() const override;
		[[nodiscard]] std::string                             str() const override;

		template<class T>
		requires std::is_trivially_copy_constructible_v<T> T readBytes() const {
			CORE_ASSERT(my_type->getName() != "void", "Interpreting VmValueRef bytes of type void!");
			CORE_ASSERT(
				sizeof(T) <= static_cast<usize>(my_type->getSize()), "VmValueRef: Out of bounds read"
			);

			auto view = memory->getPointerData(pointed_data, sizeof(T));
			return vm::safeReadPointerBytes<T>(view.getBegin());
		}
	};
}

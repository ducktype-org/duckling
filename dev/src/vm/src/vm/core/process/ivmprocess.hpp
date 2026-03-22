#pragma once

#include "interface_types.hpp"

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/memory/pointer.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>

#include <expected>

namespace vm {

	class Memory;
	class ProcIO;
	class VmValue;
	class GIL;
	class SynchronizationPrimitives;

	/**
	 * @brief The API for using the virtual process of the VM.
	 * It manages process's data, loader and threads.
	 *
	 * IVMProcess is an abstract concept that represents the program's execution environment.
	 */
	class IVMProcess {
	public:
		virtual ~IVMProcess() = default;

		virtual void setStatus(const api::ProcStatus& new_status) noexcept = 0;

		virtual Memory& getMemory() = 0;

		virtual ProcIO& getIO() = 0;

		/**
		 * @brief Entry point to perform requests on the process.
		 */
		virtual std::expected<api::Response, api::ApiError> doRequest(
			const api::RequestVariant& request
		) = 0;

		/**
		 * @brief Get the PID of the process.
		 */
		[[nodiscard]] virtual PID getPID() const = 0;

		/**
		 * @brief Creates a VmValue of a given type and registers it in this IVMProcess
		 * The VmValue is owned by the IVMProcess. VmValues created with this function are freed
		 * when the process is deinitialized.
		 *
		 * @param type The type of the data stored in the newly created VmValue.
		 * @param src The pointer to the data used to fill the newly created VmValue. If not
		 * specified, created VmValue will be empty.
		 * @return A non-owning, modifiable reference to the new VmValue.
		 */
		virtual Ref<VmValue> createVmValue(TypeCRef type)              = 0;
		virtual Ref<VmValue> createVmValue(TypeCRef type, Pointer src) = 0;

		/**
		 * @brief Creates a VmValue of a given type and transfers ownership to the caller.
		 * The caller is expected to free the VmValue.
		 *
		 * @param type The type of the data stored in the newly created VmValue.
		 * @param src The pointer to the data used to fill the newly created VmValue. If not
		 * specified, created VmValue will be empty.
		 * @return A Box referencing the newly created VmValue.
		 */
		virtual Box<VmValue> createOwnedVmValue(TypeCRef type)              = 0;
		virtual Box<VmValue> createOwnedVmValue(TypeCRef type, Pointer src) = 0;

		virtual GIL&                       getGIL()                       = 0;
		virtual SynchronizationPrimitives& getSynchronizationPrimitives() = 0;
	};
}

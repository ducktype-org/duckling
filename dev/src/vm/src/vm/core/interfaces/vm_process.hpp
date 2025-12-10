#pragma once

#include "vm/api/data/api_error.hpp"
#include "vm/api/data/request.hpp"
#include "vm/api/data/response.hpp"

#include <expected>

namespace vm {
	class NewVMProcess {
	private:
		PID my_pid;

	public:
		virtual ~NewVMProcess() = default;

		void setStatus(const api::ProcStatus& new_status) noexcept {
			{
				std::unique_lock<std::shared_mutex> lock(rw_status);
				status = new_status;
			}

			status_cv.notify_all();
		}

		/**
		 * @brief Entry point to perform requests on the process.
		 */
		std::expected<api::Response, api::ApiError> doRequest(const api::RequestVariant& request);

		[[nodiscard]] PID getPID() const { return my_pid; }

		/**
		 * @brief Creates a VmValue of a given type and registers it in this VMProcess
		 * The VmValue is owned by the VMProcess. VmValues created with this function are freed when
		 * the process is deinitialized.
		 *
		 * @param type The type of the data stored in the newly created VmValue.
		 * @param src The pointer to the data used to fill the newly created VmValue. If not
		 * specified, created VmValue will be empty.
		 * @return A non-owning, modifiable reference to the new VmValue.
		 */
		Ref<VmValue> createVmValue(TypeCRef type);
		Ref<VmValue> createVmValue(TypeCRef type, Pointer src);

		/**
		 * @brief Creates a VmValue of a given type and transfers ownership to the caller.
		 * The caller is expected to free the VmValue.
		 *
		 * @param type The type of the data stored in the newly created VmValue.
		 * @param src The pointer to the data used to fill the newly created VmValue. If not
		 * specified, created VmValue will be empty.
		 * @return A Box referencing the newly created VmValue.
		 */
		Box<VmValue> createOwnedVmValue(TypeCRef type);
		Box<VmValue> createOwnedVmValue(TypeCRef type, Pointer src);

		NewVMProcess(PID my_pid): my_pid(my_pid) {}
	};
}

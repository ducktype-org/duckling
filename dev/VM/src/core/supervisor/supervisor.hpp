#pragma once

#include <base/optional.hpp>
#include <listener/listener.hpp>
#include <filesystem/file.hpp>
#include <core/process/vmprocess.hpp>
#include <api/api.hpp>
#include <base/smart_pointers.hpp>

namespace vm {
	/**
	 * @brief Supervisor for the VM, responsible for managing VCPU's ('VCPU') and forwarding
	 * requests.
	 *
	 * To run the VM, you should use the frontend API (like `server.hpp` or `cli.hpp`), not this
	 * class directly.
	 */
	class Supervisor {
	private:
		Supervisor() = default;

		std::shared_mutex                                    rwProcessTable;
		PID                                                  next = 0;
		std::unordered_map<PID, base::unique_ptr<VMProcess>> processTable;

		cpp::result<base::borrow_ptr<VMProcess>, api::ApiError> getProcess(PID pid);


	public:
		static Supervisor& get();

		// Each of the following methods should synchronize access to the processTable, but should
		// not synchronize usage of each of the processes. Each process synchronizes its resources
		// by itself
		cpp::result<PID, api::ApiError>           newProcess();
		cpp::result<api::Response, api::ApiError> doRequest(const api::SupervisorRequest& request);
		cpp::result<void, api::ApiError>          killProcess(PID pid);
	};
}

#include "supervisor.hpp"
#include <core/process/vmprocess.hpp>
#include <mutex>

namespace vm {
	Supervisor& Supervisor::get() {
		static Supervisor supervisor = Supervisor();
		return supervisor;
	}

	cpp::result<base::borrow_ptr<VMProcess>, api::ApiError> Supervisor::getProcess(PID pid) {
		std::shared_lock lock(rwProcessTable);
		if (!processTable.contains(pid))
			return cpp::failure(api::ProcessError{ api::ProcessNotFound{} });
		return processTable.at(pid).borrow_mut();
	}

	cpp::result<PID, api::ApiError> Supervisor::newProcess() {
		std::unique_lock lock(rwProcessTable);
		processTable.emplace(next, new VMProcess());
		return next++;
	}

	cpp::result<api::Response, api::ApiError>
		Supervisor::doRequest(const api::SupervisorRequest& request) {
		return getProcess(request.pid).flat_map([&request](base::borrow_ptr<VMProcess> process) {
			return process->doRequest(request.request).map_error([](auto& x) {
				return api::ApiError{ x };
			});
		});
	}

	cpp::result<void, api::ApiError> Supervisor::killProcess(PID pid) {
		std::unique_lock lock(rwProcessTable);
		if (!processTable.contains(pid))
			return cpp::failure(api::ProcessError{ api::ProcessNotFound{} });

		processTable.erase(pid);
		return {};
	}
}

#include "supervisor.hpp"

#include <mutex>

namespace vm {
	Supervisor& Supervisor::get() {
		static Supervisor supervisor = Supervisor();
		return supervisor;
	}

	result<base::borrow_ptr<VCPU>, api::ApiError> Supervisor::getProcess(PID pid) {
		std::shared_lock lock(rwProcessTable);
		if (!processTable.contains(pid))
			return failure(api::ProcessError{ api::ProcessNotFound{} });
		return processTable.at(pid).borrow_mut();
	}

	result<PID, api::ApiError> Supervisor::newProcess(bool usesStdio) {
		std::unique_lock lock(rwProcessTable);
		processTable.emplace(next, new VCPU(usesStdio));
		return next++;
	}

	result<api::Response, api::ApiError>
		Supervisor::doRequest(const api::SupervisorRequest& request) {
		return getProcess(request.pid).flat_map([&request](base::borrow_ptr<VCPU> process) {
			return process->doRequest(request.request).map_error([](auto& x) {
				return api::ApiError{ x };
			});
		});
	}

	result<void, api::ApiError> Supervisor::killProcess(PID pid) {
		std::unique_lock lock(rwProcessTable);
		if (!processTable.contains(pid))
			return failure(api::ProcessError{ api::ProcessNotFound{} });

		processTable.erase(pid);
		return {};
	}
}

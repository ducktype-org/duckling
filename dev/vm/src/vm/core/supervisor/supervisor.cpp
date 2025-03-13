#include "supervisor.hpp"

#include <mutex>
#include <vm/core/process/vmprocess.hpp>

namespace vm {
	Supervisor& Supervisor::get() {
		static Supervisor supervisor = Supervisor();
		return supervisor;
	}

	cpp::result<Ref<VMProcess>, api::ApiError> Supervisor::getProcess(PID pid) {
		std::shared_lock lock(rwProcessTable);
		if (!processTable.contains(pid))
			return cpp::failure(api::ProcessError{ api::ProcessNotFound{} });
		return processTable.at(pid).refMut();
	}

	cpp::result<PID, api::ApiError> Supervisor::newProcess() {
		std::unique_lock lock(rwProcessTable);
		processTable.emplace(next, makeBox<VMProcess>());
		return next++;
	}

	cpp::result<api::Response, api::ApiError>
		Supervisor::doRequest(const api::SupervisorRequest& request) {
		return getProcess(request.pid).flat_map([&request](Ref<VMProcess> process) {
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

#include "supervisor.hpp"

#include <vm/core/process/vmprocess.hpp>

#include <mutex>

namespace vm {
	Supervisor& Supervisor::get() {
		static Supervisor supervisor = Supervisor();
		return supervisor;
	}

	cpp::result<Ref<VMProcess>, api::ApiError> Supervisor::getProcess(PID pid) {
		std::shared_lock lock(rw_process_table);
		if (!process_table.contains(pid))
			return cpp::failure(api::ProcessError{ api::ProcessNotFound{} });
		return process_table.at(pid).refMut();
	}

	cpp::result<PID, api::ApiError> Supervisor::newProcess() {
		std::unique_lock lock(rw_process_table);
		process_table.emplace(next, makeBox<VMProcess>());
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
		std::unique_lock lock(rw_process_table);
		if (!process_table.contains(pid))
			return cpp::failure(api::ProcessError{ api::ProcessNotFound{} });

		process_table.erase(pid);
		return {};
	}
}

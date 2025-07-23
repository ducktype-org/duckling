#include "supervisor.hpp"

#include <vm/core/process/vmprocess.hpp>

#include <mutex>

namespace vm {
	Supervisor& Supervisor::get() {
		static Supervisor supervisor = Supervisor();
		return supervisor;
	}

	std::expected<Ref<VMProcess>, api::ApiError> Supervisor::getProcess(PID pid) {
		std::shared_lock lock(rw_process_table);
		if (!process_table.contains(pid))
			return std::unexpected(api::ProcessError{ api::ProcessNotFound{} });
		return process_table.at(pid).refMut();
	}

	std::expected<PID, api::ApiError> Supervisor::newProcess() {
		std::unique_lock lock(rw_process_table);
		PID pid = next++;
		process_table.emplace(pid, makeBox<VMProcess>(pid));
		return pid;
	}

	std::expected<api::Response, api::ApiError> Supervisor::doRequest(
		const api::SupervisorRequest& request
	) {
		return getProcess(request.pid).and_then([&request](Ref<VMProcess> process) {
			return process->doRequest(request.request).transform_error([](const auto& x) {
				return api::ApiError{ x };
			});
		});
	}

	std::expected<void, api::ApiError> Supervisor::killProcess(PID pid) {
		std::unique_lock lock(rw_process_table);
		if (!process_table.contains(pid))
			return std::unexpected(api::ProcessError{ api::ProcessNotFound{} });

		process_table.erase(pid);
		return {};
	}
}

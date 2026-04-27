#include "supervisor.hpp"

#include <vm/core/process/vmprocess.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>

#include <mutex>

namespace vm {
	Supervisor& Supervisor::get() {
		static Supervisor supervisor = Supervisor();
		return supervisor;
	}

	std::expected<Ref<IVMProcess>, api::ApiError> Supervisor::getProcess(PID pid) {
		std::shared_lock lock(rw_process_table);
		if (!process_table.contains(pid)) return std::unexpected(api::ProcessNotFound{});
		return process_table.at(pid).refMut();
	}

	std::expected<PID, api::ApiError> Supervisor::newProcess() {
		std::unique_lock lock(rw_process_table);
		PID              pid = next++;
		process_table.emplace(pid, Box<IVMProcess>::fromPointer(new SafeVMProcess(pid)));
		return pid;
	}

	std::expected<api::Response, api::ApiError> Supervisor::doRequest(
		const api::SupervisorRequest& request
	) {
		variant_match(request.request) {
			variant_case_novalue(api::request::DeinitAndValidate) {
				auto res = getProcess(request.pid).and_then([](Ref<IVMProcess> process) {
					return process->doRequest(api::request::DeinitAndValidate{});
				});
				std::unique_lock lock(rw_process_table);
				process_table.erase(request.pid);
				return res;
			}
			variant_default {
				return getProcess(request.pid).and_then([&request](Ref<IVMProcess> process) {
					return process->doRequest(request.request).transform_error([](const auto& x) {
						return api::ApiError{ x };
					});
				});
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<void, api::ApiError> Supervisor::killProcess(PID pid) {
		std::unique_lock lock(rw_process_table);
		if (!process_table.contains(pid)) return std::unexpected(api::ProcessNotFound{});

		process_table.erase(pid);
		return {};
	}

	Supervisor::~Supervisor() {
		// @TODO: #1354 add asserts here, that the processes are stopped and if not then cerr the
		// warnings about it.
		for (auto& [pid, proc]: process_table) proc->doRequest(api::request::DeinitAndValidate{});
	}
}

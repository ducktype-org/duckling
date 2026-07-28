#include "supervisor.hpp"

#include <vm/api/data/status.hpp>
#include <vm/core/fast/fast_vmprocess.hpp>
#include <vm/core/process/ivmprocess.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>

#include <iostream>
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

	std::expected<PID, api::ApiError> Supervisor::newProcess(const api::ProcessConfig& options) {
		std::unique_lock lock(rw_process_table);
		PID              pid = PID::fromU64(next_pid++);
		switch (options.mode) {
		case api::ProcessMode::Safe:
			process_table.emplace(
				pid,
				Box<IVMProcess>::fromPointer(
					new SafeVMProcess(pid, options.enable_deadlock_detection)
				)
			);
			break;
		case api::ProcessMode::Fast:
			process_table.emplace(pid, Box<IVMProcess>::fromPointer(new fast::FastVMProcess(pid)));
			break;
		}
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
				// Remove the process once it has been successfully deinitialized.
				if (res) {
					// We have to destroy the process after releasing `rw_process_table`, since
					// destroying a process destroys its status Emitter (which takes the Emitter
					// lock), and the event-emission takes the locks in the opposite order (Emitter
					// lock -> rw_process_table), so destroying while holding `rw_process_table` can
					// deadlock.
					base::MBox<IVMProcess> dying_process;
					{
						std::unique_lock lock(rw_process_table);
						auto             it = process_table.find(request.pid);
						if (it != process_table.end()) {
							dying_process = std::move(it->second);
							process_table.erase(it);
						}
					}
				}
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
		// We have to destroy the process after releasing `rw_process_table`, since
		// destroying a process destroys its status Emitter (which takes the Emitter
		// lock), and the event-emission takes the locks in the opposite order (Emitter
		// lock -> rw_process_table), so destroying while holding `rw_process_table` can
		// deadlock.
		base::MBox<IVMProcess> dying_process;
		{
			std::unique_lock lock(rw_process_table);
			auto             it = process_table.find(pid);
			if (it == process_table.end()) return std::unexpected(api::ProcessNotFound{});

			dying_process = std::move(it->second);
			process_table.erase(it);
		}
		return {};
	}

	Supervisor::~Supervisor() {
		for (auto& [pid, proc]: process_table) {
			// Every process must be stopped or finished before the Supervisor is destroyed.
			auto status = proc->doRequest(api::request::StatusRequest{});
			if (status && isExecuting(v_get(*status, api::ProcStatus))) {
				std::cerr << "Supervisor destroyed while process " << pid
						  << " is still executing; stopping it now. Processes must be stopped or "
							 "finished before the Supervisor is destroyed.\n";
				(void) proc->doRequest(api::request::Stop{});
			}
			(void) proc->doRequest(api::request::DeinitAndValidate{});
		}
	}
}

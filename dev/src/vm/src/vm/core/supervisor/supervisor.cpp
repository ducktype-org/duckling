#include "supervisor.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <vm/api/data/api_error.hpp>
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
		// Reserve the PID under the lock, but build the process outside of it, since constructing a
		// VMProcess is expensive (loader, compiler, memory, the main VMThread) and holding
		// `rw_process_table` blocks every other API call.
		const PID pid = [&] {
			std::unique_lock lock(rw_process_table);
			return PID::fromU64(next_pid++);
		}();

		Box<IVMProcess> process = [&] {
			switch (options.mode) {
			case api::ProcessMode::Safe:
				return Box<IVMProcess>::fromPointer(
					new SafeVMProcess(pid, options.enable_deadlock_detection)
				);
			case api::ProcessMode::Fast:
				return Box<IVMProcess>::fromPointer(new fast::FastVMProcess(pid));
			}
			CORE_UNREACHABLE();
		}();

		{
			std::unique_lock lock(rw_process_table);
			process_table.emplace(pid, std::move(process));
		}
		return pid;
	}

	std::expected<api::Response, api::ApiError> Supervisor::doRequest(
		const PID pid, const api::RequestVariant& request
	) {
		variant_match(request) {
			variant_case_novalue(api::request::DeinitAndValidate) {
				auto res = getProcess(pid).and_then([](Ref<IVMProcess> process) {
					return process->doRequest(api::request::DeinitAndValidate{});
				});

				// If the deinit was successful, we destroy the process here. Otherwise we leave it
				// in the supervisor and let someone kill by hand.
				if (res.has_value()) (void) killProcess(pid);
				return res;
			}
			variant_default {
				return getProcess(pid).and_then([&request](Ref<IVMProcess> process) {
					return process->doRequest(request).transform_error([](const auto& x) {
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
		// lock -> listener -> API call -> rw_process_table), so destroying while holding
		// `rw_process_table` can deadlock.
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
		// Every process left here is force killed.
		for (auto& [pid, proc]: process_table) {
			auto status = proc->doRequest(api::request::StatusRequest{});
			if (status)
				std::cerr << "Supervisor destroyed while process " << pid
						  << " still exists. Processes should be deinitialized before "
							 "the Supervisor is destroyed.\n";
			(void) proc->doRequest(api::request::Stop{});
		}
		process_table.clear();
	}
}

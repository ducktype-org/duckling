#include "debugger.hpp"

#include <poll.h>

#include <vm/api/vm.hpp>

namespace vm::debugger {
	Debugger::Debugger(const fs::File& filepath, const std::vector<std::string>& main_args):
		  main_args(main_args),
		  updater([&](const vm::api::ProcStatus& status) { 
			std::lock_guard lk(queue_m);
			statuses.push(status);
			queue_cv.notify_one();	
		}) {
		const auto process_pid_response = vm::api::spawn();
		CORE_ASSERT(process_pid_response.has_value(), "Failed to spawn VM process for debugger");
		pid = process_pid_response->pid;

		const auto attach_listener_response = vm::api::attachListener(pid, updater);
		CORE_ASSERT(attach_listener_response.has_value(), "Failed to attach listener for debugger");

		const auto load_files_response = vm::api::loadFiles(pid, { filepath });
		CORE_ASSERT(load_files_response.has_value(), "Failed to load files to the VM for debugger");
	}

	void Debugger::runMain() {
		auto status = vm::api::getExecutionStatus(pid);
		CORE_ASSERT(status.has_value(), "Failed to get execution status for debugger");

		if (std::holds_alternative<vm::api::ExecutionCompleted>(status.value())) {
			const auto join_response = vm::api::join(pid);
			CORE_ASSERT(join_response.has_value(), "Failed to join VM process for debugger");
		}
		const auto run_response = vm::api::run(pid);
		CORE_ASSERT(run_response.has_value(), "Failed to run VM");
	}

	vm::api::ProcStatus Debugger::getStatus() const {
		auto response = vm::api::getExecutionStatus(pid);
		CORE_ASSERT(response.has_value(), "Failed to get execution status for debugger");
		return response.value();
	}

	bool Debugger::isNewUpdate() const { return !statuses.empty(); }

	void Debugger::updateStatus() {
		std::unique_lock lk(queue_m);
		if (queue_cv.wait_for(lk, std::chrono::milliseconds(10), [&]{ return isNewUpdate(); })) {
			on_vm_status_change.emitEvent(statuses.front());
			statuses.pop();
		}
	}
}

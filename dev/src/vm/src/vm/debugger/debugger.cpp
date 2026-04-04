#include "debugger.hpp"

#include <poll.h>

#include <vm/api/vm.hpp>

namespace vm::debugger {
	Debugger::Debugger(const fs::File& filepath, const std::vector<std::string>& main_args):
		  main_args(main_args), updater([&](vm::api::ProcStatus status){statuses.push(status);}) {
		const auto process_pid_response = vm::api::spawn();

		if (!process_pid_response.has_value())
			throw std::runtime_error("Failed to spawn VM process for debugger");
		pid = process_pid_response->pid;

		if (!vm::api::attachListener(pid, updater))
			throw std::runtime_error("Failed to attach listener for debugger")

		if (!vm::api::loadFiles(pid, { filepath }))
			throw std::runtime_error("Failed to load file into debugger");
	}

	void Debugger::runMain() {
		auto status = vm::api::getExecutionStatus(pid);
		if (!status.has_value())
			throw std::runtime_error("Failed to get execution status for debugger");
		if (std::holds_alternative<vm::api::ExecutionCompleted>(status.value())) {
			throw std::runtime_error(
				"Cannot run the process because it has already completed execution"
			);
			if (!vm::api::join(pid))
				throw std::runtime_error("Failed to join VM process for debugger");
		}
		if (!vm::api::run(pid)) throw std::runtime_error("Failed to run VM");
	}

	vm::api::ProcStatus Debugger::getStatus() const {
		auto response = vm::api::getExecutionStatus(pid);
		if (!response.has_value())
			throw std::runtime_error("Failed to get execution status for debugger");

		return response.value();
	}

	bool Debugger::isNewUpdate() const {
		return !statuses.empty();
	}

	void Debugger::updateStatus() {
		if (isNewUpdate())
			onVmStateChange.emitEvent(statuses.front());
			statuses.pop();
	}
}

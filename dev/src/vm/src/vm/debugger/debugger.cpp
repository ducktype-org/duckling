#include "debugger.hpp"

#include <poll.h>

#include <vm/api/vm.hpp>

namespace vm::debugger {
	Debugger::Debugger(const fs::File& filepath, const std::vector<std::string>& main_args):
		  main_args(main_args),
		  updater([&](const vm::api::ProcStatus& status) { on_vm_status_change.emitEvent(status); }
	      ) {
		vm::api::spawn()
			.and_then([&](const vm::api::ProcessInfo& info) {
				pid = info.pid;

				return std::expected<void, vm::api::ApiError>{};
			})
			.and_then([&] { return vm::api::loadFiles(pid, { filepath }); })
			.and_then([&] { return vm::api::attachStatusListener(pid, &updater); })
			.transform_error([](const vm::api::ApiError& api_error) -> void* {
				throw std::runtime_error(vm::api::errorToString(api_error));
			});
	}

	void Debugger::attachOnVMStatusChangeListener(Ref<events::Listener<vm::api::ProcStatus>> listener
	) {
		on_vm_status_change.attachListener(listener);
	}

	void Debugger::attachOnVMStatusChangeListener(events::Listener<vm::api::ProcStatus>& listener) {
		on_vm_status_change.attachListener(listener);
	}

	void Debugger::runMain() {
		vm::api::getExecutionStatus(pid)
			.and_then([&](const vm::api::ProcStatus& status) {
				if (std::holds_alternative<vm::api::ExecutionCompleted>(status))
					return vm::api::join(pid);
				return std::expected<void, vm::api::ApiError>{};
			})
			.and_then([&] { return vm::api::run(pid); })
			.transform_error([](const vm::api::ApiError& api_error) -> void* {
				throw std::runtime_error(vm::api::errorToString(api_error));
			});
	}

	vm::api::ProcStatus Debugger::getStatus() const {
		return vm::api::getExecutionStatus(pid)
		    .transform_error([](const vm::api::ApiError& api_error) -> void* {
				throw std::runtime_error(vm::api::errorToString(api_error));
			})
		    .value();
	}

}

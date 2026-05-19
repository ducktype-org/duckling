#include "debugger.hpp"

#include <poll.h>

#include <vm/api/vm.hpp>

inline std::string statusToString(const vm::api::ProcStatus& status) {
	return std::visit(
		[](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;
			return TypeParseTraits<T>::NAME.data();
		},
		status
	);
}

namespace vm::debugger {
	Debugger::Debugger(const std::vector<std::string>& main_args):
		  main_args(main_args),
		  updater([&](const vm::api::ProcStatus& status) {
			  on_vm_changes_status.emitEvent(status);
			  variant_match(status) {
				  variant_case(vm::api::ExecutionCompleted, completed) {
					  on_vm_completes_execution.emitEvent(completed.exit_value);
				  }
				  variant_case(vm::api::ExecutionPanicked, panicked) {
					  on_error.emitEvent(panicked.error_message);
				  }
			  }
		  }) {
		vm::api::spawn()
			.and_then([&](const vm::api::ProcessInfo& info) {
				pid = info.pid;

				return std::expected<void, vm::api::ApiError>{};
			})
			.and_then([&] { return vm::api::attachStatusListener(pid, &updater); })
			.transform_error([&](const vm::api::ApiError& api_error) {
				throw std::runtime_error(vm::api::errorToString(api_error));
				return api_error;
			});
	}

	Debugger::Debugger(const fs::File& filepath, const std::vector<std::string>& main_args):
		  Debugger(main_args) {
		loadFile(filepath);
	}

	Debugger::~Debugger() {
		updater.detach();
		vm::api::getExecutionStatus(pid)
			.and_then([&](const vm::api::ProcStatus& status) {
				if (!std::holds_alternative<api::NotStarted>(status))
					return std::expected<void, vm::api::ApiError>{};

				return std::expected<void, vm::api::ApiError>{ std::unexpected(vm::api::ApiError{
					vm::api::OtherError{ "VM was not even runned..." } }) };
			})
			.and_then([&] { return vm::api::kill(pid); })
			.transform_error([&](const vm::api::ApiError& api_error) {
				on_error.emitEvent(vm::api::errorToString(api_error));
				return api_error;
			});
	}

	void Debugger::attachOnVMChangesStatusListener(events::Listener<vm::api::ProcStatus>& listener) {
		on_vm_changes_status.attachListener(listener);
	}

	void Debugger::attachOnVMCompletesExecutionListener(events::Listener<vm::api::ExitValue>& listener
	) {
		on_vm_completes_execution.attachListener(listener);
	}

	void Debugger::attachOnErrorListener(events::Listener<std::string>& listener) {
		on_error.attachListener(listener);
	}

	void Debugger::runMain() {
		vm::api::getExecutionStatus(pid)
			.and_then([&](const vm::api::ProcStatus& status) {
				variant_match(status) {
					variant_case_novalue(vm::api::ExecutionCompleted) { return vm::api::join(pid); }
					variant_case_novalue(vm::api::NotStarted) {
						return std::expected<void, vm::api::ApiError>{};
					}
				}

				return std::expected<void, vm::api::ApiError>{ std::unexpected(vm::api::ApiError{
					vm::api::OtherError{
						"Wrong VM state to run: got " + statusToString(status)
						+ ", allowed states are NotStarted and ExecutionCompleted." } }) };
			})
			.and_then([&] { return vm::api::run(pid, main_args); })
			.transform_error([&](const vm::api::ApiError& api_error) {
				on_error.emitEvent(vm::api::errorToString(api_error));
				return api_error;
			});
	}

	vm::api::ProcStatus Debugger::getStatus() {
		return vm::api::getExecutionStatus(pid)
		    .transform_error([&](const vm::api::ApiError& api_error) {
				throw std::runtime_error(vm::api::errorToString(api_error));
				return api_error;
			})
		    .value();
	}

	void Debugger::loadFile(const fs::File& filepath) {
		auto result = vm::api::loadFiles(pid, { filepath });
		if (!result.has_value()) throw std::runtime_error(vm::api::errorToString(result.error()));
	}

	void Debugger::pause() {
		vm::api::getExecutionStatus(pid)
			.and_then([&](const vm::api::ProcStatus& status) {
				if (std::holds_alternative<vm::api::Running>(status))
					return std::expected<void, vm::api::ApiError>{};

				return std::expected<void, vm::api::ApiError>{ std::unexpected(vm::api::ApiError{
					vm::api::OtherError{ "Wrong VM state to pause: got " + statusToString(status)
				                         + ", allowed state is Running." } }) };
			})
			.and_then([&] { return vm::api::pause(pid); })
			.transform_error([&](const vm::api::ApiError& api_error) {
				on_error.emitEvent(vm::api::errorToString(api_error));
				return api_error;
			});
	}

	void Debugger::resume() {
		vm::api::getExecutionStatus(pid)
			.and_then([&](const vm::api::ProcStatus& status) {
				if (std::holds_alternative<vm::api::Paused>(status))
					return std::expected<void, vm::api::ApiError>{};

				return std::expected<void, vm::api::ApiError>{ std::unexpected(vm::api::ApiError{
					vm::api::OtherError{ "Wrong VM state to resume: got " + statusToString(status)
				                         + ", allowed state is Paused." } }) };
			})
			.and_then([&] { return vm::api::resume(pid); })
			.transform_error([&](const vm::api::ApiError& api_error) {
				on_error.emitEvent(vm::api::errorToString(api_error));
				return api_error;
			});
	}
}

#include "debugger.hpp"

#include <poll.h>

#include <vm/api/vm.hpp>

inline std::string status_to_string(const vm::api::ProcStatus& status){
	return std::visit(
		[](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;
			return TypeParseTraits<T>::NAME.data();
		},
		status
	);
}

namespace vm::debugger {
	Debugger::Debugger(const fs::File& filepath, const std::vector<std::string>& main_args):
		  main_args(main_args),
		  updater([&](const vm::api::ProcStatus& status) {
			variant_match(status) {
				variant_case(vm::api::ExecutionCompleted, completed){
					std::stringstream out;
					const auto type_name = completed.exit_value->type->getName();

					if (type_name == base::StrID("void"))
						out << "<void>";
					else if (type_name == base::StrID("i64"))
						out << completed.exit_value->readBytes<i64>();
					else if (type_name == base::StrID("i32"))
						out << completed.exit_value->readBytes<i32>() << " (as i32)\n";
					else if (type_name == base::StrID("i16"))
						out << completed.exit_value->readBytes<i16>() << " (as i16)\n";
					else if (type_name == base::StrID("byte"))
						out << completed.exit_value->readBytes<char>() << " (as byte)\n";
					else
						out << "<Unable to interpret>\n";
					on_vm_completes_execution.emitEvent(out.str());
				}
			}
			on_vm_changes_status.emitEvent(status_to_string(status));
		}
	      ) {
		vm::api::spawn()
			.and_then([&](const vm::api::ProcessInfo& info) {
				pid = info.pid;

				return std::expected<void, vm::api::ApiError>{};
			})
			.and_then([&] { return vm::api::loadFiles(pid, { filepath }); })
			.and_then([&] { return vm::api::attachStatusListener(pid, &updater); })
			.transform_error([&](const vm::api::ApiError& api_error) {
				on_error.emitEvent(vm::api::errorToString(api_error));
				return api_error;
			});
	}

	Debugger::~Debugger() {
		updater.detach();
		vm::api::kill(pid)
		.transform_error([&](const vm::api::ApiError& api_error) {
			on_error.emitEvent(vm::api::errorToString(api_error));
			return api_error;
		});
	}

	void Debugger::attachOnVMChangesStatusListener(Ref<events::Listener<std::string>> listener
	) {
		on_vm_changes_status.attachListener(listener);
	}

	void Debugger::attachOnVMChangesStatusListener(events::Listener<std::string>& listener) {
		on_vm_changes_status.attachListener(listener);
	}

	void Debugger::attachOnVMCompletesExecutionListener(Ref<events::Listener<std::string>> listener) {
		on_vm_completes_execution.attachListener(listener);
	}

	void Debugger::attachOnVMCompletesExecutionListener(events::Listener<std::string>& listener) {
		on_vm_completes_execution.attachListener(listener);
	}

	void Debugger::attachOnErrorListener(Ref<events::Listener<std::string>> listener) {
		on_error.attachListener(listener);
	}

	void Debugger::attachOnErrorListener(events::Listener<std::string>& listener){
		on_error.attachListener(listener);
	}

	void Debugger::runMain() {
		vm::api::getExecutionStatus(pid)
			.and_then([&](const vm::api::ProcStatus& status) {
				if (std::holds_alternative<vm::api::ExecutionCompleted>(status))
					return vm::api::join(pid);
				return std::expected<void, vm::api::ApiError>{};
			})
			.and_then([&] { return vm::api::run(pid, main_args); })
			.transform_error([&](const vm::api::ApiError& api_error) {
				on_error.emitEvent(vm::api::errorToString(api_error));
				return api_error;
			});
	}

	std::string Debugger::getStatus() {
		auto result = vm::api::getExecutionStatus(pid)
		    .transform([&](const vm::api::ProcStatus& status) {
				return status_to_string(status);
			});
		
		if (result.has_value())
			return result.value();
		else {
			auto err = vm::api::errorToString(result.error());
			on_error.emitEvent(err);
			return err;
		}
	}

}

#include "debugger.hpp"

#include <poll.h>

#include <base/extend_cpp/variant_match.hpp>

#include "vm/api/data/status.hpp"
#include "vm/api/data/thread_id.hpp"
#include "vm/bytecode/validator/valid_type/finalized_kinds.hpp"
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
			  variant_match(status) {
				  variant_case(vm::api::ExecutionCompleted, completed) {
					  on_vm_completes_execution.emitEvent(completed.exit_value);
				  }
				  variant_case(vm::api::ExecutionPanicked, panicked) {
					  on_error.emitEvent(panicked.error_message);
				  }
				  variant_case_novalue(vm::api::Paused) { clearEnumeratedVariablesReferences(); }
			  }
			  on_vm_changes_status.emitEvent(status);
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
		vm::api::kill(pid).transform_error([&](const vm::api::ApiError& api_error) {
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

	void Debugger::clearEnumeratedVariablesReferences() {
		enumerated_variables_references.clear();
		enumerated_variables_references.push_back(VariablesReference{
			.id = 0, .vr = VariablesReference::Nothing{} });
	}

	std::vector<StackFrameInfo> Debugger::enumerateFrames(u64 thread_id) {
		auto tid           = api::ThreadID(0);
		auto response_nosf = vm::api::debuggerGetNumberOfStackFrames(pid, tid);
		if (!response_nosf.has_value()) return {};

		u64 number_of_stack_frames = response_nosf.value().number_of_stack_frames;
		std::vector<StackFrameInfo> frames_info;
		for (u64 frame_index = 0; frame_index < number_of_stack_frames; frame_index++) {
			auto response_sfv = vm::api::debuggerGetStackFrameData(pid, tid, frame_index);
			if (!response_sfv.has_value()) return {};

			// check if we already have reference to this frame
			auto it = std::find_if(
				enumerated_variables_references.begin(),
				enumerated_variables_references.end(),
				[thread_id, frame_index](const VariablesReference& vr) {
					return std::holds_alternative<StackFrameHook>(vr.vr)
				        && std::get<StackFrameHook>(vr.vr).thread_id == thread_id
				        && std::get<StackFrameHook>(vr.vr).frame_id == frame_index;
				}
			);

			if (it != enumerated_variables_references.end()) {
				frames_info.push_back(StackFrameInfo{ .frame_id = frame_index,
				                                      .function_name
				                                      = response_sfv.value().function_name,
				                                      .variables_reference = it->id });
			} else {
				u64 variables_reference
					= enumerated_variables_references.size();

				frames_info.push_back(StackFrameInfo{ .frame_id = frame_index,
				                                      .function_name
				                                      = response_sfv.value().function_name,
				                                      .variables_reference = variables_reference });

				enumerated_variables_references.push_back(VariablesReference{
					.id = variables_reference,
					.vr = StackFrameHook{ .thread_id = thread_id, .frame_id = frame_index } });
			}
		}
		return frames_info;
	}

	std::vector<VariableInfo> Debugger::dereferenceVariablesReference(u64 variables_reference) {
		auto it = std::find_if(
			enumerated_variables_references.begin(),
			enumerated_variables_references.end(),
			[variables_reference](const VariablesReference& vr) {
				return vr.id == variables_reference;
			}
		);

		if (it == enumerated_variables_references.end())
			throw std::runtime_error("Invalid variables reference");

		std::vector<VariableInfo> variables_info;

		auto var_vars_ref = [&](vm::api::response::StackFrameData::FrameVar var) -> u64 {
			u64 var_ref = 0;

			variant_match(var.value.getType()->getKind()) {
				variant_case_novalue(
					std::monostate,
					code::valid_type::finalized::Primitive,
					code::valid_type::finalized::Opaque,
					code::valid_type::finalized::Function
				) {}
				variant_case_novalue(
					code::valid_type::finalized::Pointer,
					code::valid_type::finalized::FixedSizeTable,
					code::valid_type::finalized::DynamicTable,
					code::valid_type::finalized::Structure,
					code::valid_type::finalized::Variant
				) {
					// check if we already have reference to this variable
					auto existing_vr = std::find_if(
						enumerated_variables_references.begin(),
						enumerated_variables_references.end(),
						[&var](const VariablesReference& vr) {
							return std::holds_alternative<VariableHook>(vr.vr)
						        && std::get<VariableHook>(vr.vr).ref == var.value;
						}
					);

					if (existing_vr == enumerated_variables_references.end()) {
						var_ref
							= enumerated_variables_references.size();
						enumerated_variables_references.push_back(VariablesReference{
							.id = var_ref, .vr = VariableHook{ .ref = var.value } });
					} else {
						var_ref = existing_vr->id;
					}
					break;
				}
			}
			return var_ref;
		};

		variant_match(it->vr) {
			variant_case(StackFrameHook, hook) {
				auto response_sfv
					= vm::api::debuggerGetStackFrameData(pid, api::ThreadID(0), hook.frame_id);
				if (!response_sfv.has_value()) return {};

				for (auto& var: response_sfv.value().frame_vars) {
					variables_info.push_back(VariableInfo{
						.name                = base::StrID("var" + std::to_string(var.offset)),
						.value               = var.value.str(),
						.type                = var.value.getType()->getName().str(),
						.variables_reference = var_vars_ref(var),
					});
				}
			}
			variant_case(VariableHook, hook) {
				throw std::runtime_error("Dereferencing variable hook is not implemented yet");
			}
			variant_case_novalue(VariablesReference::Nothing) { return {}; }
		}

		return variables_info;
	}

}

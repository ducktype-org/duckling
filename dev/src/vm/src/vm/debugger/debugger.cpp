#include "debugger.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <vm/api/vm.hpp>

namespace {
	inline std::string statusToString(const vm::api::ProcStatus& status) {
		return std::visit(
			[](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				return TypeParseTraits<T>::NAME.data();
			},
			status
		);
	}
}

namespace vm::debugger {
	Debugger::Debugger(const std::vector<std::string>& main_args):
		  main_args(main_args),
		  updater([&](const api::ProcStatus& status) {
			  on_status_changed.emitEvent(status);
			  variant_match(status) {
				  variant_case(api::ExecutionPanicked, panicked) {
					  on_error.emitEvent(panicked.error_message);
				  }
			  }
		  }) {
		api::spawn()
			.and_then([&](const api::ProcessInfo& info) {
				pid = info.pid;
				return api::attachStatusListener(pid, &updater);
			})
			.transform_error([&](const api::ApiError& api_error) -> std::monostate {
				throw std::runtime_error(api::errorToString(api_error));
			});
	}

	Debugger::Debugger(const fs::File& filepath, const std::vector<std::string>& main_args):
		  Debugger(main_args) {
		loadFile(filepath).transform_error([&](const api::ApiError& api_error) -> std::monostate {
			throw std::runtime_error(api::errorToString(api_error));
		});
	}

	Debugger::~Debugger() {
		updater.detach();

		// @TODO: #1222 Remove checking status and always kill after fixing kill

		api::getExecutionStatus(pid)
			.and_then([&](const api::ProcStatus& status) {
				if (!std::holds_alternative<api::NotStarted>(status)) return api::kill(pid);

				return std::expected<void, api::ApiError>{};
			})
			.transform_error([&](const api::ApiError& api_error) {
				on_error.emitEvent(api::errorToString(api_error));
				return api_error;
			});
	}

	void Debugger::attachOnStatusChangedListener(events::Listener<api::ProcStatus>& listener) {
		on_status_changed.attachListener(listener);
	}

	void Debugger::attachOnErrorListener(events::Listener<std::string>& listener) {
		on_error.attachListener(listener);
	}

	std::expected<void, api::ApiError> Debugger::runMain() {
		return api::getExecutionStatus(pid)
		    .and_then([&](const api::ProcStatus& status) -> std::expected<void, api::ApiError> {
				variant_match(status) {
					variant_case_novalue(api::ExecutionCompleted) { return api::join(pid); }
					variant_case_novalue(api::NotStarted) { return {}; }
					variant_default {
						return std::unexpected(api::ApiError{ api::OtherError{
							"Wrong VM state to run: got " + statusToString(status)
							+ ", allowed states are NotStarted and ExecutionCompleted." } });
					}
				}
			})
		    .and_then([&] { return api::run(pid, main_args); });
	}

	api::ProcStatus Debugger::getStatus() {
		// will never fail when pid is correct
		return api::getExecutionStatus(pid)
		    .transform_error([&](const api::ApiError& api_error) -> std::monostate {
				throw std::runtime_error(api::errorToString(api_error));
			})
		    .value();
	}

	std::expected<void, api::ApiError> Debugger::loadFile(const fs::File& filepath) {
		return api::loadFiles(pid, { filepath });
	}

	std::expected<u64, api::ApiError> Debugger::getNumberOfStackFrames(api::ThreadID thread_id) {
		return api::debuggerGetNumberOfStackFrames(pid, thread_id)
		    .transform([](api::response::NumberOfCurrentStackFrames nosf) {
				return nosf.number_of_stack_frames;
			});
	}

	std::expected<api::response::StackFrameData, api::ApiError> Debugger::getStackFrameData(
		api::ThreadID thread_id, u64 frame_index
	) {
		return api::debuggerGetStackFrameData(pid, thread_id, frame_index);
	}

	std::expected<api::response::CodePosition, api::ApiError> Debugger::pause() {
		return api::getExecutionStatus(pid).and_then(
			[&](const api::ProcStatus& status
		    ) -> std::expected<api::response::CodePosition, api::ApiError> {
				if (std::holds_alternative<api::Running>(status)) return api::pause(pid);
				return std::unexpected(api::ApiError{
					api::OtherError{ "Wrong VM state to pause: got " + statusToString(status)
			                         + ", allowed state is Running." } });
			}
		);
	}

	std::expected<void, api::ApiError> Debugger::resume() {
		return api::getExecutionStatus(pid)
		    .and_then([&](const api::ProcStatus& status) -> std::expected<void, api::ApiError> {
				if (std::holds_alternative<api::Paused>(status)) return {};

				return std::unexpected(api::ApiError{
					api::OtherError{ "Wrong VM state to resume: got " + statusToString(status)
			                         + ", allowed state is Paused." } });
			})
		    .and_then([&] { return api::resume(pid); });
	}

	std::expected<api::response::CodePosition, api::ApiError> Debugger::getCurrentPosition() {
		return api::getCurrentPosition(pid);
	}

	std::expected<void, api::ApiError> Debugger::setBreakpoint(
		base::StrID function_name, u64 instr_number, bool enabled
	) {
		return api::setBreakpoint(pid, function_name, instr_number, enabled);
	}

	std::expected<void, api::ApiError> Debugger::setBreakpoint(
		fs::File file, usize line, bool enabled
	) {
		api::mapFileLineToCodeCollectionPosition(pid, file, line)
			.and_then([&](api::response::CodePosition& pos) {
				return api::setBreakpoint(pid, pos.function_name, pos.instr_number, enabled);
			});
	}

	std::expected<void, api::ApiError> Debugger::step() { return api::step(pid); }
}

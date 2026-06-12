#include "vmprocess.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <vm/api/data/response.hpp>

namespace vm {

	IVMProcess::IVMProcess(const PID my_pid): my_pid(my_pid) {}

	ProcIO& IVMProcess::getIO() { return io; }

	PID IVMProcess::getPID() const { return my_pid; }

	std::expected<api::Response, api::ApiError> IVMProcess::doRequest(
		const api::RequestVariant& request
	) {
		variant_match(request) {
			variant_case(api::request::Run, run_request) {
				return runFunction("main", run_request.program_args);
			}

			variant_case(api::request::RunFunction, run_func_request) {
				return runFunction(run_func_request.func_name, run_func_request.func_args);
			}

			variant_case(api::request::Join, join_request) { return join(join_request.thread_id); }

			variant_case(api::request::RunFunctionAwait, run_func_await_request) {
				return runFunctionAwait(
					run_func_await_request.func_name, run_func_await_request.func_args
				);
			}

			variant_case(api::request::Pause, pause_request) {
				auto response = pauseVMThread(pause_request.thread_id);
				if (response) return std::unexpected(*response);
				return getVMThreadCurrentPosition(pause_request.thread_id);
			}

			variant_case(api::request::Resume, resume_request) {
				auto response = resumeVMThread(resume_request.thread_id);
				if (response) return std::unexpected(*response);
				return api::Response(api::response::Empty());
			}

			variant_case_novalue(api::request::Step) {
				auto response = stepVMThread(getMainThreadID());
				if (response) return std::unexpected(*response);
				return getVMThreadCurrentPosition(getMainThreadID());
			}

			variant_case(api::request::LoadFiles, load_request) {
				return loadProgram(load_request.filenames).transform_error([](auto err) {
					return api::ApiError{ err };
				});
			}

			variant_case(api::request::LoadCode, load_request) {
				return loadProgram(load_request.code_collection).transform_error([](auto err) {
					return api::ApiError{ err };
				});
			}

			variant_case_novalue(api::request::Stop) { return stop(); }

			variant_case_novalue(api::request::ExecutionPosition) {
				return getVMThreadCurrentPosition(getMainThreadID());
			}

			variant_case_novalue(api::request::WaitForBreakpoint) {
				waitForBreakpoint();
				api::ProcStatus stat = getStatus();
				if (!std::holds_alternative<api::Paused>(stat))
					return std::unexpected(api::ApiError{
						api::OtherError{ "unexpected status response" } });

				return getVMThreadCurrentPosition(getMainThreadID());
			}

			variant_case(api::request::Input, input_request) { return input(input_request); }

			variant_case_novalue(api::request::Output) { return output(); }

			variant_case(api::request::Attach, attach_request) {
				return attach(attach_request.istream, attach_request.ostream);
			}

			variant_case_novalue(api::request::Detach) { return detach(); }

			variant_case(api::request::TypeMetadata, type_request) {
				return getTypeMetadata(type_request.type_name);
			}

			variant_case(api::request::VmValue, vmvalue_request) {
				return getVMValueForType(vmvalue_request.type_name);
			}

			variant_case(api::request::DebuggerGetNumberOfCurrentStackFrames, request) {
				return getNumberOfCurrentStackFrames(request.thread_id);
			}

			variant_case(api::request::DebuggerGetStackFrameData, request) {
				return getStackFrameData(request.thread_id, request.frame_index);
			}

			variant_case(api::request::StatusRequest, status_request) {
				return api::Response(getStatus());
			}

			variant_case_novalue(api::request::ExitCodeRequest) { return getExitCode(); }

			variant_case_novalue(api::request::DeinitAndValidate) { return deinitAndValidate(); }

			variant_case(api::request::AttachStatusListener, request) {
				on_status_changed.attachListener(request.listener);
				return api::Response(api::response::Empty());
			}

			variant_case(api::request::AttachOutputListener, request) {
				io.attachOutputListener(request.listener);
				return api::Response(api::response::Empty());
			}

			variant_case(api::request::SetBreakpoint, request) {
				return setBreakpoint(
					request.function_name, request.instruction_index, request.enable
				);
			}

			variant_case(api::request::MapFileLineToCodeCollectionPosition, request) {
				return mapFileLineToCodeCollectionPosition(request.file, request.line_number);
			}

			variant_default { return api::Response(api::response::Empty()); }
		}

		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> IVMProcess::attach(
		std::istream& istream, std::ostream& ostream
	) {
		// @TODO: Flush the ostream from ProcIO to new ostream.
		if (io_redirecter) return std::unexpected(api::ApiError{ api::AttachDetachError{} });

		// So long this object lives, any IO is redirected.
		io_redirecter.emplace(io.attach(istream, ostream));
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> IVMProcess::detach() {
		if (!io_redirecter) return std::unexpected(api::ApiError{ api::AttachDetachError{} });
		io_redirecter.reset();
		return api::Response(api::response::Empty());
	}

	void IVMProcess::applyEvent(const lifecycle::Event& event, api::ThreadID thread_id) noexcept {
		const bool is_main_thread      = thread_id.asInt() == 0;
		const bool is_terminal_failure = std::holds_alternative<lifecycle::Panic>(event)
		                              || std::holds_alternative<lifecycle::Stop>(event);

		// Only the main thread drives the overall process status. Child threads can
		// only publish terminal failures, and only while the process is not already
		// terminal - the first failure wins.
		lifecycle::Machine::ResultT result;
		if (is_main_thread) {
			result = status_machine.handleEvent(event);
		} else {
			if (!is_terminal_failure) return;
			result = status_machine.handleEventIf(
				[](const api::ProcStatus& status) { return !api::isStatusTerminal(status); }, event
			);
		}

		if (!result.has_value() || !result.value().has_value()) return;
		const api::ProcStatus& new_status = result.value().value().state;

		// Emit after the machine's internal lock is released: observers may call
		// getStatus()/isExecutionPanicked() which take the same lock; emitting under
		// the lock would self-deadlock.
		on_status_changed.emitEvent(new_status);

		if (api::isStatusTerminal(new_status)) onTerminalStatus(new_status);
	}

	api::ProcStatus IVMProcess::getStatus() { return status_machine.getStateCopy(); }

	bool IVMProcess::isExecutionPanicked() {
		return status_machine.withState([](const api::ProcStatus& status) {
			return std::holds_alternative<api::ExecutionPanicked>(status);
		});
	}

	std::expected<api::Response, api::ApiError> IVMProcess::input(const api::request::Input& request
	) {
		// @TODO: #2342 https://github.com/ducktype-org/duckling/pull/381#discussion_r1885688218
		auto lock = io.lock();
		io.inputStream() << request.input;
		for (auto id: getAllThreadIDs()) notifyPausedVMThread(id);
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> IVMProcess::output() {
		auto lock = io.lock();
		// @TODO: #2342 Cannot read output from api when IO is being redirected
		if (io_redirecter)
			return std::unexpected(api::ApiError{
				api::IOError{ "Cannot read output from api when IO is being redirected" } });

		if (isExecuting(getStatus()))
			io.output_empty_cv.wait(lock, [&] { return !io.outputStream().str().empty(); });

		const std::string content = io.outputStream().str();
		io.outputStream().str("");
		io.outputStream().clear();
		return api::Response(api::response::Output{ content });
	}
}

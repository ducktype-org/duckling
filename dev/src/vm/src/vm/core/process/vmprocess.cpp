#include "vmprocess.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <vm/api/data/response.hpp>

namespace vm {

	IVMProcess::IVMProcess(const PID my_pid): my_pid(my_pid), status(api::NotStarted{}) {}

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

	void IVMProcess::setStatus(const api::ProcStatus& new_status, api::ThreadID thread_id) noexcept {
		const bool is_main_thread = thread_id.asInt() == 0;
		const bool is_terminal_failure = std::holds_alternative<api::ExecutionPanicked>(new_status)
		                               || std::holds_alternative<api::ExecutionStopped>(new_status);
		if (!is_main_thread){
		  	// Only main thread can set overall process status
			if(is_terminal_failure)
				// Child threads can publish terminal failures if process is not already terminal
		  		setStatusIfNotTerminal(new_status, thread_id);
		 	return;
		}

		{
			std::unique_lock<std::shared_mutex> lock(rw_status);
			status = new_status;
		}
		on_status_changed.emitEvent(new_status);
		status_cv.notify_all();
		if (api::isStatusTerminal(new_status)) onTerminalStatus(new_status);
	}

	bool IVMProcess::setStatusIfNotTerminal(
		const api::ProcStatus& new_status, api::ThreadID thread_id
	) noexcept {
		const bool is_main_thread = thread_id.asInt() == 0;

		// Child threads should not move the whole process into a terminal state.
		// They may still publish a terminal failure so the process can stop as a whole.
		if (is_main_thread) return false;

		bool            updated = false;
		api::ProcStatus emitted_status;
		{
			std::unique_lock<std::shared_mutex> lock(rw_status);
			if (!api::isStatusTerminal(status)) {
				status         = new_status;
				emitted_status = new_status;
				updated        = true;
			}
		}
		if (updated) {
			on_status_changed.emitEvent(emitted_status);
			status_cv.notify_all();
			if (api::isStatusTerminal(emitted_status)) onTerminalStatus(emitted_status);
		}
		return updated;
	}

	api::ProcStatus IVMProcess::getStatus() {
		std::shared_lock lock(rw_status);
		return status;
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

		if (isExecuting(status))
			io.output_empty_cv.wait(lock, [&] { return !io.outputStream().str().empty(); });

		const std::string content = io.outputStream().str();
		io.outputStream().str("");
		io.outputStream().clear();
		return api::Response(api::response::Output{ content });
	}
}

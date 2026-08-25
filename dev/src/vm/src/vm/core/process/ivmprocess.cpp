#include "ivmprocess.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <vm/api/data/response.hpp>

namespace vm {
	namespace ps = process_sm::process_state;
	namespace pe = process_sm::process_event;

	IVMProcess::IVMProcess(const PID my_pid): my_pid(my_pid) {
		state_manager.setOnStatusChangedCallback([this](const ProcessState& state) {
			const api::ProcStatus status = process_sm::toApiStatus(state);
			on_status_changed.emitEvent(status);
			if (ps::isTerminal(state)) onTerminalStatus(status);
		});
	}

	void IVMProcess::applyThreadEvent(api::ThreadID tid, const ThreadEvent& event) {
		const ProcessStateManager::ApplyResult result
			= state_manager.applyThreadEventOrAbort(tid, event);
		// The first panic requests a stop of the whole process.
		if (result.stop_all_threads) requestStopAllThreads();
	}

	base::Optional<api::ApiError> IVMProcess::validateRequest(const ProcessEvent& event) const {
		const ProcessState state = state_manager.aggregate();
		bool               valid = true;

		variant_match(event) {
			variant_case_novalue(pe::Run) { valid = !v_matches(state, ps::Stopping); }
			variant_case_novalue(pe::Stop) {}
			variant_default { CORE_UNREACHABLE(); }
		}

		if (!valid)
			return api::ApiError{ api::StateError{ base::strConcat(
				"Invalid request '",
				pe::processEventName(event),
				"' in current state '",
				ps::processStateName(state),
				"'"
			) } };
		return {};
	}

	base::Optional<api::ApiError> IVMProcess::prepareRun() {
		if (not ps::isTerminal(state_manager.aggregate())) return {};
		if (auto reset = state_manager.resetForRun(); !reset.has_value())
			return api::ApiError{ api::StateError{ reset.error() } };
		return {};
	}

	api::ProcStatus IVMProcess::getStatus() { return process_sm::toApiStatus(getProcessState()); }

	ProcIO& IVMProcess::getIO() { return io; }

	PID IVMProcess::getPID() const { return my_pid; }

	std::expected<api::Response, api::ApiError> IVMProcess::doRequest(
		const api::RequestVariant& request
	) {
#define VALIDATE_REQUEST(event)                                                       \
	if (auto validation_error = validateRequest(event); validation_error.has_value()) \
		return std::unexpected(*validation_error);

		variant_match(request) {
			variant_case(api::request::Run, run_request) {
				VALIDATE_REQUEST(pe::Run{});
				if (auto e = prepareRun(); e.has_value()) return std::unexpected(*e);
				return runFunction("main", run_request.program_args);
			}

			variant_case(api::request::RunFunction, run_func_request) {
				VALIDATE_REQUEST(pe::Run{});
				if (auto e = prepareRun(); e.has_value()) return std::unexpected(*e);
				return runFunction(run_func_request.func_name, run_func_request.func_args);
			}

			variant_case(api::request::RunFunctionAwait, run_func_await_request) {
				VALIDATE_REQUEST(pe::Run{});
				if (auto e = prepareRun(); e.has_value()) return std::unexpected(*e);
				return runFunctionAwait(
					run_func_await_request.func_name, run_func_await_request.func_args
				);
			}

			variant_case(api::request::Join, join_request) { return join(join_request.thread_id); }


			variant_case(api::request::Pause, pause_request) {
				// Per-thread operation. It's validity is decided by the target thread not the process.
				auto response = pauseVMThread(pause_request.thread_id);
				if (response) return std::unexpected(*response);
				return getVMThreadCurrentPosition(pause_request.thread_id);
			}

			variant_case(api::request::Resume, resume_request) {
				// Per-thread operation. It's validity is decided by the target thread not the process.
				auto response = resumeVMThread(resume_request.thread_id);
				if (response) return std::unexpected(*response);
				return api::Response(api::response::Empty());
			}

			variant_case_novalue(api::request::Step) {
				// Per-thread operation. It's validity is decided by the target thread not the
				// process. Until `request::Step` carries a ThreadID it can only target the main
				// thread.
				auto response = stepVMThread(getMainThreadID());
				if (response) return std::unexpected(*response);
				return getVMThreadCurrentPosition(getMainThreadID());
			}

			variant_case_novalue(api::request::Stop) {
				// Always legal.
				return stop();
			}

			variant_case_novalue(api::request::WaitForBreakpoint) {
				waitForBreakpoint();
				if (!v_matches(getProcessState(), ps::Paused))
					return std::unexpected(api::ApiError{
						api::OtherError{ "Unexpected status response" } });

				return getVMThreadCurrentPosition(getMainThreadID());
			}

			// The following are read-only and don't change the state.
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

			variant_case(api::request::ExecutionPosition, current_pos_req) {
				return getVMThreadCurrentPosition(getMainThreadID(), current_pos_req.frame_idx);
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

			variant_case(api::request::VMValue, vmvalue_request) {
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

			variant_case_novalue(api::request::DeinitAndValidate) {
				if (ps::isExecuting(getProcessState()))
					return std::unexpected(api::ApiError{
						api::StateError{ "Cannot deinitialize and validate a "
					                     "process while it is still executing; "
					                     "stop or finish all threads first" } });
				return deinitAndValidate();
			}

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

			variant_case(api::request::SetExecutionConfig, request) {
				return setExecutionConfig(request.config);
			}

			variant_default { return api::Response(api::response::Empty()); }
		}
#undef VALIDATE_REQUEST
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

	std::expected<api::Response, api::ApiError> IVMProcess::input(const api::request::Input& request
	) {
		// @TODO: #2342 https://github.com/ducktype-org/duckling/pull/381#discussion_r1885688218
		auto lock = io.lock();
		io.inputStream() << request.input;
		for (auto id: getAllThreadIDs()) notifyVMThreadWaiters(id);
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

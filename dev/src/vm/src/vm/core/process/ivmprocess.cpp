#include "ivmprocess.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <vm/api/data/response.hpp>

#include <ranges>

namespace vm {
	namespace ps = process_state;
	namespace pe = process_event;
	namespace ts = thread_state;

	IVMProcess::IVMProcess(const PID my_pid): my_pid(my_pid) {
		state_manager.setOnStatusChangedCallback([this](const ProcessState& state) {
			on_status_changed.emitEvent(toApiStatus(state));
		});
	}

	void IVMProcess::applyThreadEvent(api::ThreadID tid, const ThreadEvent& event) {
		const ProcessStateManager::ApplyResult result
			= state_manager.applyThreadEventOrAbort(tid, event);
		// The first panic requests a stop of the whole process.
		if (result.stop_all_threads) requestStopAllThreads();
	}

	std::expected<void, api::ApiError> IVMProcess::validateProcessRequest(const ProcessEvent& event
	) const {
		const ProcessState          state = state_manager.aggregate();
		base::Optional<std::string> invalid_reason;

		variant_match(event) {
			variant_case_novalue(pe::Run) {
				// Run can be performed only if the previous run was completed successfully. `Stopper`
				// or `Panicked` means the DVM was left in an undefined state and can't be reused.
				if (!v_matches(state, ps::NotStarted, ps::Completed))
					invalid_reason
						= "the process must be freshly loaded or completed successfully in the "
						  "previous run";
				else if (const std::vector<api::ThreadID> unjoined = unjoinedThreadIds();
				         !unjoined.empty()) {
					// We also require every thread of the previous run to be joined.
					std::string ids = base::strJoin(
						unjoined | std::views::transform([](const api::ThreadID id) {
							return std::to_string(id.asInt());
						}),
						", "
					);
					invalid_reason = base::strConcat(
						unjoined.size() == 1 ? "thread " : "threads ",
						ids,
						unjoined.size() == 1 ? " of the previous run was never joined"
											 : " of the previous run were never joined"
					);
				}
			}
			variant_case_novalue(pe::Stop) {}
			variant_case_novalue(pe::DeinitAndValidate) {
				if (ps::isExecuting(state)) invalid_reason = "the process is still executing";
			}
			variant_default { CORE_UNREACHABLE(); }
		}

		if (invalid_reason.has_value())
			return std::unexpected(api::ApiError{ api::StateError{ base::strConcat(
				"Invalid request '",
				pe::processEventName(event),
				"' in state '",
				ps::processStateName(state),
				"': ",
				*invalid_reason
			) } });
		return {};
	}

	std::expected<void, api::ApiError> IVMProcess::prepareRun() {
		if (not ps::isTerminal(state_manager.aggregate())) return {};
		if (auto reset = state_manager.resetForRun(); !reset.has_value())
			return std::unexpected(api::ApiError{ api::StateError{ reset.error() } });
		return {};
	}

	api::ProcStatus IVMProcess::getStatus() { return toApiStatus(getProcessState()); }

	std::vector<api::ThreadID> IVMProcess::pauseAllVMThreads() {
		using ThreadState = thread_state::ThreadState;

		std::vector<api::ThreadID> paused;
		std::vector<api::ThreadID> awaited;

		// First ask all threads to pause.
		for (const api::ThreadID tid: getAllActiveThreadIDs()) {
			const ThreadState state = state_manager.threadState(tid);
			// Already paused.
			if (v_matches(state, ts::Paused)) {
				paused.push_back(tid);
				continue;
			}

			// Thread can't be paused, so we skip it.
			if (requestPauseOfVMThread(tid).has_value()) awaited.push_back(tid);
		}

		// Now wait for all threads that need awaiting to be paused.
		for (const api::ThreadID tid: awaited) {
			const ThreadState state
				= state_manager.waitForThreadState(tid, [](const ThreadState& s) {
					  return v_matches(s, ts::Paused) || ts::isTerminal(s);
				  });
			if (v_matches(state, ts::Paused)) paused.push_back(tid);
		}
		return paused;
	}

	ProcIO& IVMProcess::getIO() { return io; }

	PID IVMProcess::getPID() const { return my_pid; }

	std::expected<api::Response, api::ApiError> IVMProcess::doRequest(
		const api::RequestVariant& request
	) {
#define VALIDATE_REQUEST(event)                                                   \
	if (auto validation = validateProcessRequest(event); !validation.has_value()) \
		return std::unexpected(validation.error());

#define VALIDATE_RUN_ARGUMENTS(func_name, args)                                           \
	if (auto validation = validateRunArguments(func_name, args); !validation.has_value()) \
		return std::unexpected(validation.error());

		variant_match(request) {
#define HANDLE_RUN_CASE(REQUEST, FUN_NAME, CALLBACK, ARGS)                            \
	variant_case(api::request::REQUEST, req) {                                        \
		VALIDATE_REQUEST(pe::Run{});                                                  \
		VALIDATE_RUN_ARGUMENTS(FUN_NAME, ARGS);                                       \
		if (auto e = prepareRun(); !e.has_value()) return std::unexpected(e.error()); \
		return CALLBACK(FUN_NAME, ARGS);                                              \
	}

			HANDLE_RUN_CASE(Run, "main", runFunction, req.program_args);
			HANDLE_RUN_CASE(RunAwait, "main", runFunctionAwait, req.program_args);
			HANDLE_RUN_CASE(RunFunction, req.func_name, runFunction, req.func_args);
			HANDLE_RUN_CASE(RunFunctionAwait, req.func_name, runFunctionAwait, req.func_args);

			variant_case(api::request::Join, join_request) { return join(join_request.thread_id); }


			variant_case(api::request::Pause, pause_request) {
				// Per-thread operation. Its validity is decided by the target thread not the process.
				auto response = pauseVMThread(pause_request.thread_id);
				if (!response) return std::unexpected(response.error());
				return getVMThreadCurrentPosition(pause_request.thread_id);
			}

			variant_case_novalue(api::request::PauseAll) {
				// Per-thread operation. Its validity is decided by the target thread not the process.
				return api::Response(api::response::ThreadIDs{ pauseAllVMThreads() });
			}

			variant_case(api::request::Resume, resume_request) {
				// Per-thread operation. Its validity is decided by the target thread not the process.
				auto response = resumeVMThread(resume_request.thread_id);
				if (!response) return std::unexpected(response.error());
				return api::Response(api::response::Empty());
			}

			variant_case(api::request::Step, step_request) {
				// Per-thread operation. Its validity is decided by the target thread not the process.
				auto response = stepVMThread(step_request.thread_id);
				if (!response) return std::unexpected(response.error());
				return getVMThreadCurrentPosition(step_request.thread_id);
			}

			variant_case(api::request::WaitForBreakpoint, wait_request) {
				// Per-thread operation. Its validity is decided by the target thread not the process.
				return waitForBreakpointAndReportPosition(wait_request.thread_id);
			}

			variant_case_novalue(api::request::DeinitAndValidate) {
				VALIDATE_REQUEST(pe::DeinitAndValidate{});
				return deinitAndValidate();
			}

			variant_case_novalue(api::request::Stop) {
				// Always legal.
				return stop();
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
				return getVMThreadCurrentPosition(api::MAIN_THREAD_ID, current_pos_req.frame_idx);
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
#undef HANDLE_RUN_CASE
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
		for (auto id: getAllActiveThreadIDs()) notifyVMThreadWaiters(id);
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

#include "ivmprocess.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <vm/api/data/response.hpp>

namespace vm {
	namespace ps = process_sm::process_state;
	namespace pe = process_sm::process_event;

	IVMProcess::IVMProcess(const PID my_pid): my_pid(my_pid) {}

	void IVMProcess::adoptThread(IVMThread& thread) {
		const api::ThreadID tid = thread.getThreadID();

		{
			// Set the initial state of the VMThread.
			std::lock_guard lock(threads_states_mutex);
			threads_states.setThreadState(tid, thread.getThreadState());
			last_process_state = process_sm::aggregate(threads_states);
		}

		// Subscribe with a thread. VMThread's state machine will call `onThreadStateChanged` each
		// time a VMThread state changes.
		thread.getStateMachine().subscribe(
			[this, tid](const IVMThread::ThreadState&, const IVMThread::ThreadState& to, u64) {
				onThreadStateChanged(tid, to);
			}
		);
	}

	void IVMProcess::onThreadStateChanged(
		api::ThreadID tid, const thread_sm::thread_state::ThreadState& new_state
	) {
		ps::ProcessState prev, next;
		bool             kill_all_threads = false;

		{
			std::lock_guard lock(threads_states_mutex);
			prev = last_process_state;

			// Terminal process states are not further changeable.
			if (ps::isTerminal(prev)) return;

			threads_states.setThreadState(tid, new_state);
			next = process_sm::aggregate(threads_states);

			// On first panic, send a Kill to all threads.
			if (v_matches(next, ps::Panicked) && !v_matches(prev, ps::Panicked)) {
				threads_states.stop_requested = true;
				next                          = process_sm::aggregate(threads_states);
				kill_all_threads              = true;
			}

			last_process_state = next;
			process_state_change_counter++;
		}

		threads_states_changed.notify_all();

		if (prev.index() != next.index()) {
			const api::ProcStatus status = process_sm::toApiStatus(next);
			on_status_changed.emitEvent(status);
			if (ps::isTerminal(next)) onTerminalStatus(status);
		}

		if (kill_all_threads) requestStopAllThreads();
	}

	base::Optional<api::ApiError> IVMProcess::applyCommand(const ProcessEvent& event) {
		bool             send_stop_to_all_threads = false;
		ps::ProcessState prev_state;
		ps::ProcessState new_state;

		{
			std::lock_guard lock(threads_states_mutex);
			prev_state = last_process_state;
			bool valid = true;

			variant_match(event) {
				variant_case_novalue(pe::Run) {
					valid = !v_matches(last_process_state, ps::Stopping);
					if (valid) {
						threads_states.stop_requested = false;
						if (ps::isTerminal(last_process_state)) {
							for (auto& [tid, state]: threads_states.threads)
								state = thread_sm::thread_state::NotStarted{};
							last_process_state = process_sm::aggregate(threads_states);
							process_state_change_counter++;
						}
					}
				}
				variant_case_novalue(pe::Pause) {
					valid = v_matches(last_process_state, ps::Running);
				}
				variant_case_novalue(pe::Resume) {
					valid = v_matches(last_process_state, ps::Paused);
				}
				variant_case_novalue(pe::Step) {
					valid = v_matches(last_process_state, ps::Paused);
				}
				variant_case_novalue(pe::Stop) {
					if (!ps::isTerminal(last_process_state)) {
						threads_states.stop_requested = true;
						last_process_state            = process_sm::aggregate(threads_states);
						process_state_change_counter++;
						send_stop_to_all_threads = true;
					}
				}
				variant_default { CORE_UNREACHABLE(); }
			}

			if (!valid)
				return api::ApiError{ api::StateError{ base::strConcat(
					"Invalid request '",
					pe::processEventName(event),
					"in current state '",
					ps::processStateName(last_process_state),
					"'"
				) } };

			new_state = last_process_state;
		}

		if (send_stop_to_all_threads) {
			threads_states_changed.notify_all();
			if (prev_state.index() != new_state.index()) {
				const api::ProcStatus status = process_sm::toApiStatus(new_state);
				on_status_changed.emitEvent(status);
				if (ps::isTerminal(new_state)) onTerminalStatus(status);
			}
			requestStopAllThreads();
		}

		return {};
	}

	IVMProcess::ProcessState IVMProcess::getProcessState() const {
		std::lock_guard lock(threads_states_mutex);
		return process_sm::aggregate(threads_states);
	}

	api::ProcStatus IVMProcess::getStatus() { return process_sm::toApiStatus(getProcessState()); }

	ProcIO& IVMProcess::getIO() { return io; }

	PID IVMProcess::getPID() const { return my_pid; }

	std::expected<api::Response, api::ApiError> IVMProcess::doRequest(
		const api::RequestVariant& request
	) {
#define APPLY_COMMAND(event) \
	if (auto e = applyCommand(event); e.has_value()) return std::unexpected(*e);
		variant_match(request) {
			variant_case(api::request::Run, run_request) {
				APPLY_COMMAND(pe::Run{});
				return runFunction("main", run_request.program_args);
			}

			variant_case(api::request::RunFunction, run_func_request) {
				APPLY_COMMAND(pe::Run{});
				return runFunction(run_func_request.func_name, run_func_request.func_args);
			}

			variant_case(api::request::Join, join_request) { return join(join_request.thread_id); }

			variant_case(api::request::RunFunctionAwait, run_func_await_request) {
				APPLY_COMMAND(pe::Run{});
				return runFunctionAwait(
					run_func_await_request.func_name, run_func_await_request.func_args
				);
			}

			variant_case(api::request::Pause, pause_request) {
				APPLY_COMMAND(pe::Pause{});
				auto response = pauseVMThread(pause_request.thread_id);
				if (response) return std::unexpected(*response);
				return getVMThreadCurrentPosition(pause_request.thread_id);
			}

			variant_case(api::request::Resume, resume_request) {
				APPLY_COMMAND(pe::Resume{});
				auto response = resumeVMThread(resume_request.thread_id);
				if (response) return std::unexpected(*response);
				return api::Response(api::response::Empty());
			}

			variant_case_novalue(api::request::Step) {
				APPLY_COMMAND(pe::Step{});
				auto response = stepVMThread(getMainThreadID());
				if (response) return std::unexpected(*response);
				return getVMThreadCurrentPosition(getMainThreadID());
			}
			variant_case_novalue(api::request::Stop) {
				APPLY_COMMAND(pe::Stop{});
				return stop();
			}

			variant_case_novalue(api::request::WaitForBreakpoint) {
				waitForBreakpoint();
				api::ProcStatus stat = getStatus();
				if (!v_matches(stat, api::Paused))
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


			variant_case_novalue(api::request::ExecutionPosition) {
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

			variant_case_novalue(api::request::DeinitAndValidate) {
				// @TODO: #2966 Don't use `api::ProcStatus` here. Use `ProcessState` instead.
				if (isExecuting(getStatus()))
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

			variant_default { return api::Response(api::response::Empty()); }
		}
#undef APPLY
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

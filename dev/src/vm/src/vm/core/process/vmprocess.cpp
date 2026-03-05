#include "vmprocess.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include "vm/debugger/vm_debug_symb.hpp"
#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/core/thread/vmvalue.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/logger.hpp>

#include <expected>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <variant>

namespace vm {
	Memory& VMProcess::getMemory() { return memory; }

	api::ProcStatus VMProcess::getStatus() {
		std::shared_lock lock(rw_status);
		return status;
	}

	template<loader::LoadMode load_mode>
	std::expected<api::Response, api::LoadProgramError> VMProcess::loadProgram(
		const std::variant<std::vector<fs::File>, code::CodeCollection>& source
	) {
		std::unique_lock                          lock(rw_global);
		std::expected<void, loader::LoaderLogger> code_result = [&] {
			variant_match(source) {
				variant_case(std::vector<fs::File>, files) {
					return loader.loadAndCompile<load_mode>(files);
				}
				variant_case(code::CodeCollection, code) {
					return loader.loadAndCompile<load_mode>(code);
				}
			}
			CORE_UNREACHABLE();
		}();

		if (code_result.has_value()) {
			return api::Response(api::response::Empty());
		} else {
			std::stringstream ss;
			code_result.error().dump(ss);
			std::cerr << ss.str() << '\n';
			return std::unexpected(api::LoadProgramError{ "Error in loader: \n" + ss.str() });
		}
	}

	std::expected<api::Response, api::OtherError> VMProcess::putBreakpoint(
		u64 func_id, u64 instr_pos
	) {
		auto& instr
			= const_cast<MicroInstruction&>(loaded_program->getFunctions()[func_id].bc[instr_pos]);

		auto orig_opcode = getInstructionOpcode(instr);

		if (orig_opcode == low::MicroOpcode::breakpoint)
			return std::unexpected{ api::OtherError{
				.error = "Breakpoint already present at given position" } };

		known_breakpoints.put(&instr, orig_opcode);

		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::OtherError> VMProcess::removeBreakpoint(
		u64 func_id, u64 instr_pos
	) {
		auto& instr
			= const_cast<MicroInstruction&>(loaded_program->getFunctions()[func_id].bc[instr_pos]);

		if (getInstructionOpcode(instr) != low::MicroOpcode::breakpoint)
			return std::unexpected{ api::OtherError{
				.error = "There is no breakpoint at given position" } };

		auto orig_opcode = known_breakpoints.atMaybeCopy(&instr);
		if (orig_opcode.empty()) {
			return std::unexpected{ api::OtherError{
				.error = "original opcode of the instruction not found" } };
		}

		instr = makeLowInstruction(*orig_opcode, instr.arg0, instr.arg1);

		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> VMProcess::runFunction(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(rw_global);

		bool response
			= getMainVMThread().spawnThreadAndRun(loaded_program, func_name, run_arguments);
		if (!response) return std::unexpected(api::ApiError{ api::RunError{} });

		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> VMProcess::join() {
		// @TODO: check status
		auto& thread          = getMainVMThread();
		auto& opt_exec_thread = thread.exec_thread;
		if (!opt_exec_thread || !opt_exec_thread->joinable())
			return std::unexpected(api::ApiError{ api::JoinError{} });

		opt_exec_thread->join();
		opt_exec_thread.reset();

		auto execution_status = thread.execution_response_queue.pop();
		variant_match(execution_status) {
			variant_case(api::ExecutionCompleted, completed) {
				return api::Response(api::response::Empty());
			}
			variant_case(api::ExecutionPanicked, panicked) {
				return std::unexpected(api::ApiError(
					api::OtherError("Execution panicked with error: " + panicked.error_message)
				));
			}
			variant_default {
				return std::unexpected(api::ApiError(api::OtherError("Unexpected run status!")));
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> VMProcess::input(const api::request::Input& request
	) {
		// @TODO: https://github.com/ducktype-org/duckling/pull/381#discussion_r1885688218
		auto lock = io.lock();
		io.inputStream() << request.input;
		getMainVMThread().notifyPaused();
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> VMProcess::output() {
		auto lock = io.lock();
		// Cannot read output from api when IO is being redirected
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

	std::expected<api::Response, api::ApiError> VMProcess::stop() {
		auto& thread          = getMainVMThread();
		auto  response        = thread.stop();
		auto& opt_exec_thread = thread.exec_thread;

		if (opt_exec_thread && opt_exec_thread->joinable()) {
			opt_exec_thread->join();
			getMainVMThread().exec_thread.reset();
		} else {
			return std::unexpected(api::ApiError{ api::JoinError{} });
		}

		// @TODO: make two different "stop" functions, one that throws error if program panicked
		if (!response)
			return std::unexpected(api::ApiError{ api::OtherError{ "unexpected status response" } });
		return api::response::Empty{};
	}

	base::Optional<api::ApiError> VMProcess::assertProcessCanRespond() {
		api::ProcStatus status = getStatus();

		if (std::holds_alternative<api::Parsing>(status)
		    || std::holds_alternative<api::TypeAnalysis>(status)) {
			return api::ApiError{ api::OtherError{
				"Cannot do memory request while parsing or analyzing types" } };
		}

		if (std::holds_alternative<api::Running>(status))
			return api::ApiError{ api::OtherError{
				"Cannot do memory request while program is running" } };

		return {};
	}

	std::expected<api::Response, api::ApiError> VMProcess::doRequest(
		const api::RequestVariant& request
	) {
		// @note This function should be thread safe, as it is called from the outside world.
		// Each function that modifies internal state should be performed under the appropriate lock.
		variant_match(request) {
			variant_case(api::request::Run, run_request) {
				return runFunction("main", run_request.program_args);
			}

			variant_case(api::request::RunFunction, run_func_request) {
				return runFunction(run_func_request.func_name, run_func_request.func_args);
			}

			variant_case_novalue(api::request::Join) { return join(); }

			variant_case_novalue(api::request::Pause) {
				auto response = getMainVMThread().pause();
				if (!response) return std::unexpected(api::ApiError{ api::PauseError{} });
				return getMainVMThread().getCurrentPosition();
			}

			variant_case_novalue(api::request::Resume) {
				auto response = getMainVMThread().resume();
				if (!response) return std::unexpected(api::ApiError{ api::ResumeError{} });
				return api::Response(api::response::Empty());
			}

			variant_case_novalue(api::request::Step) {
				auto response = getMainVMThread().step();
				if (!response)
					return std::unexpected(api::ApiError{
						api::OtherError{ "step error" },
					});
				return getMainVMThread().getCurrentPosition();
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
				return getMainVMThread().getCurrentPosition();
			}

			variant_case_novalue(api::request::WaitForBreakpoint) {
				std::shared_lock lock(rw_status);
				status_cv.wait(lock, [&] {
					return std::holds_alternative<api::Paused>(status)
					    || api::isStatusTerminal(status);
				});

				if (!std::holds_alternative<api::Paused>(status))
					return std::unexpected(api::ApiError{
						api::OtherError{ "unexpected status response" } });

				return getMainVMThread().getCurrentPosition();
			}

			variant_case(api::request::Input, input_request) { return input(input_request); }

			variant_case_novalue(api::request::Output) { return output(); }

			variant_case(api::request::Attach, attach_request) {
				getMainVMThread().notifyPaused();
				return attach(attach_request.istream, attach_request.ostream);
			}

			variant_case_novalue(api::request::Detach) { return detach(); }

			variant_case(api::request::TypeMetadata, type_request) {
				std::shared_lock lock(rw_global);
				match_optional(assertProcessCanRespond()) {
					opt_some(error) { return std::unexpected(error); }
					opt_none {
						auto res = loaded_program->getTypes().atMaybe(
							base::StrID(type_request.type_name.c_str())
						);
						match_optional(res) {
							opt_some(value) { return api::response::Type{ value }; }

							opt_none {
								return std::unexpected(api::ApiError{
									api::OtherError{ "Type not found" } });
							}
						}
					}
				}
			}

			variant_case(api::request::VmValue, vmvalue_request) {
				std::shared_lock lock(rw_global);
				match_optional(assertProcessCanRespond()) {
					opt_some(error) { return std::unexpected(error); }
					opt_none {
						auto maybe_type = loaded_program->getTypes().atMaybe(
							base::StrID(vmvalue_request.type_name.c_str())
						);
						match_optional(maybe_type) {
							opt_some(type) {
								auto vm_value = createOwnedVmValue(type);
								return api::response::VmValue{ std::move(vm_value) };
							}
							opt_none {
								return std::unexpected(api::ApiError{
									api::OtherError{ "Type not found" } });
							}
						}
					}
				}
			}

			variant_case_novalue(api::request::DebuggerGetNumberOfCurrentStackFrames) {
				RuntimeData& runtime_data = getMainVMThread().runtime_data;
				u64 frames = runtime_data.frame_stack_current - runtime_data.frame_stack_base + 1;
				return api::Response(api::response::NumberOfCurrentStackFrames{
					.number_of_stack_frames = frames });
			}

			variant_case(api::request::DebuggerGetStackFrameVars, request) {
				RuntimeData& runtime_data = getMainVMThread().runtime_data;
				Frame&       frame
					= runtime_data.frame_stack_base[request.frame_index];  // TODO: assert it's
				                                                           // a valid frame

				std::vector<api::response::StackFrameVars::FrameVar> frame_vars;
				for (auto& [offset, block_idx]: frame.local_offset_to_block_idx) {
					Ref<Block> block = frame.block_stack[block_idx];
					frame_vars.push_back(api::response::StackFrameVars::FrameVar{
						.offset  = offset,
						.pointer = Pointer(block, 0),
						.type    = memory.getBlockType(block) });
				}

				return api::Response(api::response::StackFrameVars{
					.function_name = frame.current_function->name, .frame_vars = frame_vars });
			}

			variant_case(api::request::DebuggerGetPointerData, request) {
				base::ModRawView view = memory.getPointerData(request.pointer, request.size);
				return api::Response(api::response::PointerData{ .data = view });
			}

			variant_case(api::request::DebuggerDereferencePointer, request) {
				base::ModRawView view = memory.getPointerData(request.pointer, Type::POINTER_SIZE);
				Pointer          pointer = safeReadPointerBytes<Pointer>(view.getBegin());
				return api::Response(api::response::Pointer{ .pointer = pointer });
			}

			variant_case(api::request::DebuggerLoadFiles, load_request) {
				return loadProgram<loader::LoadMode::FROM_DBC_FILE>(load_request.filenames)
				    .transform_error([](auto err) { return api::ApiError{ err }; });
			}

			variant_case(api::request::DebuggerPutBreakpoint, request) {
				return putBreakpoint(request.function_id, request.instr_number)
				    .transform_error([](auto err) { return api::ApiError{ err }; });
			}

			variant_case(api::request::DebuggerRemoveBreakpoint, request) {
				return removeBreakpoint(request.function_id, request.instr_number)
				    .transform_error([](auto err) { return api::ApiError{ err }; });
			}

			variant_case(api::request::StatusRequest, status_request) {
				return api::Response(getStatus());
			}

			variant_case_novalue(api::request::ExitCodeRequest) { return getExitCode(); }

			variant_case_novalue(api::request::DeinitAndValidate) { return deinitAndValidate(); }

			variant_default { return api::Response(api::response::Empty()); }
		}

		CORE_UNREACHABLE();
	}

	Ref<VmValue> VMProcess::createVmValue(TypeCRef type) {
		auto value = Box<VmValue>::fromPointer(new VmValue(*this, type));
		owned_vm_values.push_back(std::move(value));
		return owned_vm_values.back().refMut();
	}

	Ref<VmValue> VMProcess::createVmValue(TypeCRef type, Pointer src) {
		auto value = Box<VmValue>::fromPointer(new VmValue(*this, type, src));
		owned_vm_values.push_back(std::move(value));
		return owned_vm_values.back().refMut();
	}

	Box<VmValue> VMProcess::createOwnedVmValue(TypeCRef type) {
		return Box<VmValue>::fromPointer(new VmValue(*this, type));
	}

	Box<VmValue> VMProcess::createOwnedVmValue(TypeCRef type, Pointer src) {
		return Box<VmValue>::fromPointer(new VmValue(*this, type, src));
	}

	PID VMProcess::getPID() const { return my_pid; }

	VMProcess::VMProcess(const PID my_pid):
		  my_pid(my_pid),
		  status(api::ExecutionNotStarted{}),
		  loaded_program(loader.getProgram()) {
		vm_threads.emplace_back(*this);
	}

	VMProcess::VMProcess(const PID my_pid, const int debugger_event_fd):
		  my_pid(my_pid),
		  status(api::ExecutionNotStarted{}),
		  debugger_event_fd(debugger_event_fd),
		  loaded_program(loader.getProgram()) {
		vm_threads.emplace_back(*this);
	}

	ProcIO& VMProcess::getIO() { return io; }

	VMThread& VMProcess::getMainVMThread() { return vm_threads.front(); }

	std::expected<api::Response, api::ApiError> VMProcess::attach(
		std::istream& istream, std::ostream& ostream
	) {
		// @TODO: Flush the ostream from ProcIO to new ostream.
		if (io_redirecter) return std::unexpected(api::ApiError{ api::AttachDetachError{} });

		// So long this object lives, any IO is redirected.
		io_redirecter.emplace(io.attach(istream, ostream));
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> VMProcess::detach() {
		if (!io_redirecter) return std::unexpected(api::ApiError{ api::AttachDetachError{} });
		io_redirecter.reset();
		return api::Response(api::response::Empty());
	}

	void VMProcess::setStatus(const api::ProcStatus& new_status) noexcept {
		{
			std::unique_lock<std::shared_mutex> lock(rw_status);
			status = new_status;
		}
		status_cv.notify_all();
		if (debugger_event_fd != -1) {
			uint8_t byte = 1;
			write(debugger_event_fd, &byte, 1);
		}
	}

	std::expected<api::Response, api::StateError> VMProcess::getExitCode() {
		std::unique_lock lock(rw_global);
		variant_match(getStatus()) {
			variant_case(api::ExecutionCompleted, completed) { return completed.exit_value; }
			variant_default return std::unexpected(api::StateError(
				executingStarted(getStatus()) ? "Execution did not complete"
											  : "Execution did not start"
			));
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> VMProcess::deinitAndValidate() {
		std::unique_lock lock(rw_global);
		for (auto& t: vm_threads) {
			if (t.exec_thread)
				if (auto res = stop(); !res.has_value()) return res;
		}
		try {
			// There might be numerous runtime exceptions during the deinitialization,
			// any of those means there was an issue during the validation.
			getMainVMThread().execGlobalDestructors(loaded_program);

			for (const auto& vm_value: owned_vm_values) vm_value->freeData();

			memory.deinitGlobals();
		} catch (exceptions::VMRuntimeException& e) {
			std::cerr << " - VM has detected issues during program\'s deinitialization: "
					  << e.what() << '\n';
			return false;
		}
		return memory.validateMemoryState();
	}
}

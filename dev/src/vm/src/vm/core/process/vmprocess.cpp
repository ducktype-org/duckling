#include "vmprocess.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/thread/low_program/instruction.hpp>
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

	std::expected<api::Response, api::LoadProgramError> VMProcess::loadProgram(
		const std::variant<std::vector<fs::File>, code::CodeCollection>& source
	) {
		std::unique_lock                          lock(rw_global);
		std::expected<void, loader::LoaderLogger> code_result = [&] {
			variant_match(source) {
				variant_case(std::vector<fs::File>, files) { return loader.loadAndCompile(files); }
				variant_case(code::CodeCollection, code) { return loader.loadAndCompile(code); }
			}
			CORE_UNREACHABLE();
		}();

		if (code_result.has_value()) {
			return api::Response(api::response::Empty());
		} else {
			std::stringstream ss;
			code_result.error().dump(ss);
			return std::unexpected(api::LoadProgramError{ ss.str() });
		}
	}

	std::expected<api::Response, api::ApiError> VMProcess::runFunction(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(rw_global);
		VMThread&        thread = getEmptyThread();
		bool response = thread.spawnThreadAndRun(loaded_program, func_name, run_arguments);
		// Setting thread ctx necessary for now, until function pointers implemented
		thread.setThreadCtx("");

		if (!response) return std::unexpected(api::ApiError{ api::RunError{} });
		i64 id = static_cast<i64>(std::hash<std::thread::id>{}(thread.exec_thread->get_id()));

		return api::Response(api::ThreadID{ id });
	}

	std::expected<api::Response, api::ApiError> VMProcess::runFunctionAwait(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(rw_global);

		getMainVMThread().runNoSpawn(loaded_program, func_name, run_arguments);
		variant_match(getStatus()) {
			variant_case(api::ExecutionCompleted, completed) { return completed.exit_value; }
			variant_default return std::unexpected(api::StateError(
				executingStarted(getStatus()) ? "Execution did not complete"
											  : "Execution did not start"
			));
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> VMProcess::join(api::ThreadID thread_id) {
		// @TODO: check status
		auto& thread = getVMThreadByID(thread_id);

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
		for (auto& thread: vm_threads) {
			auto  response        = thread.stop();
			auto& opt_exec_thread = thread.exec_thread;

			if (opt_exec_thread && opt_exec_thread->joinable()) {
				opt_exec_thread->join();
				thread.exec_thread.reset();
			} else {
				return std::unexpected(api::ApiError{ api::JoinError{} });
			}

			// @TODO: #1222 make two different "stop" functions, one that throws error if program
			// panicked
			if (!response)
				return std::unexpected(api::ApiError{
					api::OtherError{ "unexpected status response" } });
		}
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

			variant_case(api::request::Join, join_request) { return join(join_request.thread_id); }

			variant_case(api::request::RunFunctionAwait, run_func_await_request) {
				return runFunctionAwait(
					run_func_await_request.func_name, run_func_await_request.func_args
				);
			}

			variant_case(api::request::Pause, pause_request) {
				auto& thread   = getVMThreadByID(pause_request.thread_id);
				auto  response = thread.pause();
				if (!response) return std::unexpected(api::ApiError{ api::PauseError{} });
				return getVMThreadByID(pause_request.thread_id).getCurrentPosition();
			}

			variant_case(api::request::Resume, resume_request) {
				auto& thread   = getVMThreadByID(resume_request.thread_id);
				auto  response = thread.resume();
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
        addNewThread();
	}

	ProcIO& VMProcess::getIO() { return io; }

	VMThread& VMProcess::getMainVMThread() { return vm_threads.front(); }

	VMThread& VMProcess::getVMThreadByID(api::ThreadID thread_id) {
        // @TODO: This case should throw an error / return optional
        if(thread_id.asInt() >= next_thread_id){
            return getMainVMThread();
        }
        return vm_threads.at(thread_id.asInt());
	}

    VMThread& VMProcess::addNewThread(){
		vm_threads.emplace_back(*this);
        return vm_threads.back();
    }

	VMThread& VMProcess::getEmptyThread() {
		for (auto& thread: vm_threads)
			if (!thread.exec_thread) return thread;

		return addNewThread();
	}

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

	GIL& VMProcess::getGIL() { return gil; }

	SynchronizationPrimitives& VMProcess::getSynchronizationPrimitives() {
		return synchronization_primitives;
	}

    i64 VMProcess::getNextThreadId(){
        return next_thread_id++;
    }
}

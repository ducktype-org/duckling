#include "safe_vmprocess.hpp"

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
	Memory& SafeVMProcess::getMemory() { return memory; }

	api::ProcStatus SafeVMProcess::getStatus() {
		std::shared_lock lock(rw_status);
		return status;
	}

	std::expected<api::Response, api::LoadProgramError> SafeVMProcess::loadProgram(
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

	std::expected<api::Response, api::ApiError> SafeVMProcess::runFunction(
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

	std::expected<api::Response, api::ApiError> SafeVMProcess::runFunctionAwait(
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

	std::expected<api::Response, api::ApiError> SafeVMProcess::join(api::ThreadID thread_id) {
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

	std::expected<api::Response, api::ApiError> SafeVMProcess::input(
		const api::request::Input& request
	) {
		auto lock = io.lock();
		io.inputStream() << request.input;
		getMainVMThread().notifyPaused();
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::output() {
		auto lock = io.lock();
		if (io_redirecter)
			return std::unexpected(api::ApiError{
				api::IOError{ "Cannot read output from api when IO is being redirected" } });

		if (isExecuting(status))
			io.output_empty_cv.wait(lock, [&] { return io.outputStream().rdbuf()->in_avail() > 0; });

		const std::string content = io.outputStream().str();
		io.outputStream().str("");
		io.outputStream().clear();
		return api::Response(api::response::Output{ content });
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::stop() {
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
			// was not stopped successfully and another that does nothing
			// if (!response) return std::unexpected(api::ApiError{ api::StopError{} });
		}

		return api::response::Empty{};
	}

	base::Optional<api::ApiError> SafeVMProcess::assertProcessCanRespond() {
		api::ProcStatus status = getStatus();

		if (std::holds_alternative<api::Parsing>(status)
		    || std::holds_alternative<api::TypeAnalysis>(status)) {
			return api::ApiError{ api::OtherError{ "Program is generating compiler info" } };
		}

		if (std::holds_alternative<api::Running>(status))
			return api::ApiError{ api::OtherError{ "Process is running." } };

		return {};
	}

	Ref<VmValue> SafeVMProcess::createVmValue(TypeCRef type) {
		auto value = Box<VmValue>::fromPointer(new VmValue(*this, type));
		owned_vm_values.push_back(std::move(value));
		return owned_vm_values.back().refMut();
	}

	Ref<VmValue> SafeVMProcess::createVmValue(TypeCRef type, Pointer src) {
		auto value = Box<VmValue>::fromPointer(new VmValue(*this, type, src));
		owned_vm_values.push_back(std::move(value));
		return owned_vm_values.back().refMut();
	}

	Box<VmValue> SafeVMProcess::createOwnedVmValue(TypeCRef type) {
		return Box<VmValue>::fromPointer(new VmValue(*this, type));
	}

	Box<VmValue> SafeVMProcess::createOwnedVmValue(TypeCRef type, Pointer src) {
		return Box<VmValue>::fromPointer(new VmValue(*this, type, src));
	}

	SafeVMProcess::SafeVMProcess(const PID my_pid):
		  VMProcess(my_pid),
		  status(api::ExecutionNotStarted{}),
		  loaded_program(loader.getProgram()) {
		vm_threads.emplace_back(*this);
	}

	VMThread& SafeVMProcess::getMainVMThread() { return vm_threads.front(); }

	VMThread& SafeVMProcess::getVMThreadByID(api::ThreadID thread_id) {
		if (thread_id == api::ThreadID{ 0 }) return getMainVMThread();
		for (auto& thread: vm_threads) {
			if (thread.exec_thread) {
				api::ThreadID id = static_cast<api::ThreadID>(
					std::hash<std::thread::id>{}(thread.exec_thread->get_id())
				);
				if (id == thread_id) return thread;
			}
		}
		return getMainVMThread();
	}

	VMThread& SafeVMProcess::getEmptyThread() {
		for (auto& thread: vm_threads)
			if (!thread.exec_thread) return thread;

		vm_threads.emplace_back(*this);
		return vm_threads.back();
	}

	void SafeVMProcess::setStatus(const api::ProcStatus& new_status) noexcept {
		{
			std::unique_lock<std::shared_mutex> lock(rw_status);
			status = new_status;
		}
		status_cv.notify_all();
	}

	std::expected<api::Response, api::StateError> SafeVMProcess::getExitCode() {
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

	std::expected<api::Response, api::ApiError> SafeVMProcess::deinitAndValidate() {
		std::unique_lock lock(rw_global);
		for (auto& t: vm_threads) {
			if (t.exec_thread)
				if (auto res = stop(); !res.has_value()) return res;
		}
		try {
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

	base::Optional<api::ApiError> SafeVMProcess::pauseVMThread(api::ThreadID thread_id) {
		auto& thread   = getVMThreadByID(thread_id);
		auto  response = thread.pause();
		if (!response) return api::ApiError{ api::PauseError{} };
		return {};
	}

	base::Optional<api::ApiError> SafeVMProcess::resumeVMThread(api::ThreadID thread_id) {
		auto& thread   = getVMThreadByID(thread_id);
		auto  response = thread.resume();
		if (!response) return api::ApiError{ api::ResumeError{} };
		return {};
	}

	base::Optional<api::ApiError> SafeVMProcess::stepMainVMThread() {
		auto response = getMainVMThread().step();
		if (!response) return api::ApiError{ api::OtherError{ "step error" } };
		return {};
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getVMThreadCurrentPosition(
		api::ThreadID thread_id
	) {
		return getVMThreadByID(thread_id).getCurrentPosition();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getMainVMThreadCurrentPosition() {
		return getMainVMThread().getCurrentPosition();
	}

	void SafeVMProcess::waitForBreakpoint() {
		std::shared_lock lock(rw_status);
		status_cv.wait(lock, [&] {
			return std::holds_alternative<api::Paused>(status) || api::isStatusTerminal(status);
		});
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getTypeMetadata(
		const std::string& type_name
	) {
		std::shared_lock lock(rw_global);
		match_optional(assertProcessCanRespond()) {
			opt_some(error) { return std::unexpected(error); }
			opt_none {
				auto res = loaded_program->getTypes().atMaybe(base::StrID(type_name.c_str()));
				match_optional(res) {
					opt_some(value) { return api::response::Type{ value }; }
					opt_none {
						return std::unexpected(api::ApiError{ api::OtherError{ "Type not found" } });
					}
				}
			}
		}
	}
}

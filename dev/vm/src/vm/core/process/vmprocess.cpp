#include "vmprocess.hpp"

#include "base/optional.hpp"
#include <base/exceptions.hpp>
#include <base/variant.hpp>

#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/state_error.hpp>
#include <vm/api/data/status.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/logger.hpp>

#include <expected>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <variant>

namespace vm {
	Memory& VMProcess::getMemory() { return memory; }

	ServiceManager& VMProcess::getServices() { return service_manager; }

	api::ProcStatus VMProcess::getStatus() {
		std::shared_lock lock(rw_status);
		return status;
	}

	std::expected<api::Response, api::LoadProgramError> VMProcess::loadProgram(
		const std::variant<std::vector<fs::FilePath>, code::CodeCollection>& source
	) {
		std::unique_lock                                       lock(rw_global);
		std::expected<low::LowVMProgram, loader::LoaderLogger> code_result = [&] {
			variant_match(source) {
				variant_case(std::vector<fs::FilePath>, files) { return loader->getProgram(files); }
				variant_case(code::CodeCollection, code) { return loader->getProgram(code); }
			}
			CORE_UNREACHABLE();
		}();

		if (code_result.has_value()) {
			loaded_program.emplace(std::move(code_result).value());
			return api::Response(api::response::Empty());
		} else {
			std::stringstream ss;
			code_result.error().dump(ss);
			return std::unexpected(api::LoadProgramError{ "Error in loader: \n" + ss.str() });
		}
	}

	std::expected<api::Response, api::CoreOperationError> VMProcess::runFunction(
		const std::string&              func_name,
		const std::vector<i64>&         func_args,
		const std::vector<std::string>& program_args
	) {
		std::unique_lock lock(rw_global);
		if (!loaded_program.has_value())
			return std::unexpected(api::CoreOperationError{ api::RunError{} });

		bool response = getMainVMThread().spawnThreadAndRun(
			&*loaded_program, func_name, func_args, program_args
		);
		if (!response) return std::unexpected(api::CoreOperationError{ api::RunError{} });

		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::CoreOperationError> VMProcess::join() {
		// @TODO: check status
		auto& thread          = getMainVMThread();
		auto& opt_exec_thread = thread.exec_thread;
		if (!opt_exec_thread || !opt_exec_thread->joinable())
			return std::unexpected(api::CoreOperationError{ api::JoinError{} });

		opt_exec_thread->join();
		opt_exec_thread.reset();

		auto execution_status = thread.execution_response_queue.pop();
		variant_match(execution_status) {
			variant_case(api::ExecutionCompleted, completed) {
				return api::Response(api::response::Empty());
			}
			variant_case(api::ExecutionPanicked, panicked) {
				return std::unexpected(api::CoreOperationError(api::OtherError("Execution panicked!"
				)));
			}
			variant_default {
				return std::unexpected(
					api::CoreOperationError(api::OtherError("Unexpected run status!"))
				);
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::CoreOperationError> VMProcess::input(
		const api::request::Input& request
	) {
		// @TODO: https://github.com/ducktype-org/duckling/pull/381#discussion_r1885688218
		auto lock = io.lock();
		io.inputStream() << request.input;
		getMainVMThread().notifyPaused();
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::CoreOperationError> VMProcess::output() {
		auto        lock = io.lock();
		std::string content;
		CORE_ASSERT(!io_redirecter, "Cannot read output from api when IO is being redirected");
		io.output_empty_cv.wait(lock, [&] { return !(content = io.outputStream().str()).empty(); });
		io.outputStream().str("");
		io.outputStream().clear();
		return api::Response(api::response::Output{ content });
	}

	std::expected<api::Response, api::CoreOperationError> VMProcess::stop() {
		auto& thread   = getMainVMThread();
		auto  response = thread.stop();
		auto& opt_exec_thread = thread.exec_thread;

		if (opt_exec_thread && opt_exec_thread->joinable()) {
			opt_exec_thread->join();
			getMainVMThread().exec_thread.reset();
		} else {
			return std::unexpected(api::CoreOperationError{ api::JoinError{} });
		}

		// @TODO: make two different "stop" functions, one that throws error if program panicked
		if (!response)
			return std::unexpected(api::CoreOperationError{
				api::OtherError{ "unexpected status response" } });
		return api::response::Empty{};
	}

	std::expected<api::Response, api::CoreOperationError> VMProcess::doRequest(
		const api::ExecutorRequest& request
	) {
		variant_match(request) {
			variant_case(api::request::Run, run_request) {
				return runFunction("main", {}, run_request.program_args);
			}
			variant_case(api::request::RunFunction, run_func_request) {
				return runFunction(run_func_request.func_name, run_func_request.func_args, {});
			}
			variant_case_novalue(api::request::Join) { return join(); }
			variant_case_novalue(api::request::Pause) {
				auto response = getMainVMThread().pause();
				if (!response) return std::unexpected(api::CoreOperationError{ api::PauseError{} });
				return getMainVMThread().getCurrentPosition();
			}
			variant_case_novalue(api::request::Resume) {
				auto response = getMainVMThread().resume();
				if (!response)
					return std::unexpected(api::CoreOperationError{ api::ResumeError{} });
				return api::Response(api::response::Empty());
			}
			variant_case_novalue(api::request::Step) {
				auto response = getMainVMThread().step();
				if (!response)
					return std::unexpected(api::CoreOperationError{
						api::OtherError{ "step error" },
					});
				return getMainVMThread().getCurrentPosition();
			}
			variant_case(api::request::LoadFiles, load_request) {
				return loadProgram(load_request.filenames).transform_error([](auto err) {
					return api::CoreOperationError{ err };
				});
			}
			variant_case(api::request::LoadCode, load_request) {
				return loadProgram(load_request.code_collection).transform_error([](auto err) {
					return api::CoreOperationError{ err };
				});
			}
			variant_case_novalue(api::request::Stop) { return stop(); }
			variant_case_novalue(api::request::ExecutionPosition) {
				return getMainVMThread().getCurrentPosition();
			}
			variant_case_novalue(api::request::WaitForBreakpoint) {
				std::shared_lock lock(rw_status);
				status_cv.wait(lock, [&] {
					if (!std::holds_alternative<api::Executing>(status)) return false;
					auto exec_status = std::get<api::Executing>(status).exec_status;
					return std::holds_alternative<api::Paused>(exec_status)
					    || api::isStatusTerminal(exec_status);
				});

				if (!std::holds_alternative<api::Executing>(status))
					return std::unexpected(api::CoreOperationError{
						api::OtherError{ "unexpected status response" } });

				auto exec_status = std::get<api::Executing>(status).exec_status;
				if (!std::holds_alternative<api::Paused>(exec_status))
					return std::unexpected(api::CoreOperationError{
						api::OtherError{ "unexpected status response" } });

				return getMainVMThread().getCurrentPosition();
			}
			variant_default { return api::Response(api::response::Empty()); }
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::CoreOperationError> VMProcess::doRequest(
		const api::DataRequest& request
	) {
		api::ProcStatus status = getStatus();

		if (!std::holds_alternative<api::Executing>(status)) {
			if (std::holds_alternative<api::Parsing>(status)
			    || std::holds_alternative<api::TypeAnalysis>(status)) {
				return std::unexpected(api::CoreOperationError{ api::OtherError{
					"Cannot do memory request while parsing or analyzing types" } });
			}
		}

		api::ExecStatus exec_status = std::get<api::Executing>(status).exec_status;
		if (std::holds_alternative<api::Running>(exec_status))
			return std::unexpected(api::CoreOperationError{
				api::OtherError{ "Cannot do memory request while program is running" } });
		std::expected<api::Response, api::CoreOperationError> response;
		variant_match(request) {
			variant_case(api::request::TypeMetadata, type_request) {
				auto res
					= loaded_program->types->atMaybe(base::StrID(type_request.type_name.c_str()));
				match_optional(res) {
					opt_some(value) { response = value; }
					opt_none {
						response = std::unexpected(api::CoreOperationError{
							api::OtherError{ "Type not found" } });
					}
				}
			}
			variant_case(api::request::Block, block_request) {
				response = api::Response(api::response::Block{
					memory.requestBlockData(block_request.block_id) });
			}
			variant_default { response = api::Response(api::response::Empty()); }
		}
		return response;
	}

	std::expected<api::Response, api::CoreOperationError> VMProcess::doRequest(
		const api::IORequest& request
	) {
		variant_match(request) {
			variant_case(api::request::Input, input_request) { return input(input_request); }
			variant_case_novalue(api::request::Output) { return output(); }
			variant_case(api::request::Attach, attach_request) {
				getMainVMThread().notifyPaused();
				return attach(attach_request.istream, attach_request.ostream);
			}
			variant_case_novalue(api::request::Detach) { return detach(); }
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> VMProcess::doRequest(
		const api::RequestVariant& request
	) {
		variant_match(request) {
			variant_case(api::ExecutorRequest, exec_request) { return doRequest(exec_request); }
			variant_case(api::DataRequest, data_request) { return doRequest(data_request); }
			variant_case(api::IORequest, io_request) { return doRequest(io_request); }
			variant_case(api::StatusRequest, status_request) { return api::Response(getStatus()); }
			variant_case(api::ExitCodeRequest, exit_code_request) { return getExitCode(); }
			variant_default { return api::Response(api::response::Empty()); }
		}
		CORE_UNREACHABLE();
	}

	VMProcess::VMProcess():
		  status(api::ExecutionNotStarted{}),
		  loader(makeBox<loader::Loader>(VALIDATE_CODE)) {
		vm_threads.emplace_back(*this);
	}

	VMProcess::~VMProcess() {
		for (auto& t: vm_threads)
			if (t.exec_thread) (void) (stop());
	}

	ProcIO& VMProcess::getIO() { return io; }

	VMThread& VMProcess::getMainVMThread() { return vm_threads.front(); }

	std::expected<api::Response, api::CoreOperationError> VMProcess::attach(
		std::istream& istream, std::ostream& ostream
	) {
		// @TODO: Flush the ostream from ProcIO to new ostream.
		if (io_redirecter)
			return std::unexpected(api::CoreOperationError{ api::AttachDetachError{} });

		// So long this object lives, any IO is redirected.
		io_redirecter.emplace(io.attach(istream, ostream));
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::CoreOperationError> VMProcess::detach() {
		if (!io_redirecter)
			return std::unexpected(api::CoreOperationError{ api::AttachDetachError{} });
		io_redirecter.reset();
		return api::Response(api::response::Empty());
	}

	void VMProcess::onEvent(const api::ProcStatus& event) noexcept {
		{
			std::unique_lock<std::shared_mutex> lock(rw_status);
			status = event;
		}
		status_cv.notify_all();
	}

	std::expected<api::Response, api::StateError> VMProcess::getExitCode() {
		variant_match(getStatus()) {
			variant_case(api::Executing, exec_status) {
				variant_match(exec_status.exec_status) {
					variant_case(api::ExecutionCompleted, completed) { return completed.exit_code; }
					variant_default return std::unexpected(
						api::StateError("Execution did not complete")
					);
				}
			}
			variant_default return std::unexpected(api::StateError("Execution did not start"));
		}
		CORE_UNREACHABLE();
	}
}

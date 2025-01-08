#include "vmprocess.hpp"
#include "api/data/status.hpp"
#include <api/data/core_operation_error.hpp>
#include <base/exceptions.hpp>
#include <core/process/memory/memory.hpp>
#include <preprocessor/preprocessor.hpp>
#include <core/thread/vmthread.hpp>

#include <mutex>
#include <base/variant.hpp>
#include <api/data/request.hpp>
#include <variant>

namespace vm {
	Memory& VMProcess::getMemory() { return memory; }

	TypeMetadata& VMProcess::getTypeMetadata() { return type_meta_data; }

	ServiceManager& VMProcess::getServices() { return service_manager; }

	void VMProcess::onEvent(const api::ProcStatus& event) noexcept {
		std::unique_lock lock(rwStatus);
		// @TODO: check if status change is legal
		status = event;
		status_cv.notify_all();
	}

	api::ProcStatus VMProcess::getStatus() {
		std::shared_lock lock(rwStatus);
		return status;
	}

	cpp::result<api::Response, api::LoadProgramError>
		VMProcess::loadProgram(const fs::FilePath& path) {
		std::unique_lock lock(rwGlobal);
		// @TODO: this code should be improved in the future to not just return plain strings
		auto code_result = preprocessor.getCode(path);

		if (code_result.has_value()) {
			loadedCode = base::Optional(code_result.value());
			return api::Response(api::response::Empty());
		} else {
			return cpp::failure(api::LoadProgramError{ code_result.error() });
		}
	}

	cpp::result<api::Response, api::CoreOperationError> VMProcess::run() {
		std::unique_lock lock(rwGlobal);
		if (!loadedCode.has_value())
			return cpp::failure(api::CoreOperationError{ api::RunError{} });
		getMainVMThread().initThread(Ref(&*loadedCode));
		return api::Response(api::response::Empty());
	}

	cpp::result<api::Response, api::CoreOperationError> VMProcess::join() {
		// @TODO: more verbose errors
		// @TODO: check status
		auto& thread = getMainVMThread().exec_thread;
		if (thread && thread->joinable())
			thread->join();
		else
			return cpp::failure(api::CoreOperationError{ api::AttachDetachError{} });
		return api::Response(api::response::Empty());
	}

	cpp::result<api::Response, api::CoreOperationError>
		VMProcess::input(const api::request::Input& request) {
		// @TODO: https://github.com/ducktype-org/duckling/pull/381#discussion_r1885688218
		auto lock = io.lock();
		io.inputStream() << request.input;
		getMainVMThread().notifyPaused();
		return api::Response(api::response::Empty());
	}

	cpp::result<api::Response, api::CoreOperationError> VMProcess::output() {
		auto        lock = io.lock();
		std::string content;
		CORE_ASSERT(!io_redirecter, "Cannot read output from api when IO is being redirected");
		io.output_empty_cv.wait(lock, [&] { return !(content = io.outputStream().str()).empty(); });
		return api::Response(api::response::Output{ content });
	}

	cpp::result<api::Response, api::CoreOperationError> VMProcess::stop() {
		getMainVMThread().stop();

		(void) join();
		getMainVMThread().exec_thread.reset();
		return api::response::Empty{};
	}

	cpp::result<api::Response, api::CoreOperationError>
		VMProcess::doRequest(const api::ExecutorRequest& request) {
		variant_match(request) {
			variant_case_novalue(api::request::Run) { return run(); }
			variant_case_novalue(api::request::Join) { return join(); }
			variant_case_novalue(api::request::Pause) {
				getMainVMThread().pause();
				bool is_paused = waitForPaused();
				if (!is_paused) return cpp::failure(api::CoreOperationError{ api::PauseError{} });
				return getMainVMThread().getCurrentPosition();
			}
			variant_case_novalue(api::request::Resume) {
				// @todo we can't wait for resume here, because thread maybe be already paused on the next breakpoint
				// and the status will be changed to paused again
				getMainVMThread().resume();
				return api::Response(api::response::Empty());
			}
			variant_case_novalue(api::request::Step) {
				getMainVMThread().step();
				bool is_paused = waitForPaused();
				if (!is_paused) return cpp::failure(api::CoreOperationError{ api::OtherError{} });
				return getMainVMThread().getCurrentPosition();
			}
			variant_case(api::request::Load, load_request) {
				return loadProgram(load_request.filename).map_error([](auto err) {
					return api::CoreOperationError{ err };
				});
			}
			variant_case_novalue(api::request::Stop) { return stop(); }
			variant_case_novalue(api::request::ExecutionPosition) {
				return getMainVMThread().getCurrentPosition();
			}
			variant_default { return api::Response(api::response::Empty()); }
		}
		CORE_UNREACHABLE();
	}

	cpp::result<api::Response, api::CoreOperationError>
		VMProcess::doRequest(const api::DataRequest& request) {
		std::shared_lock lock(rwStatus);
		if (!std::holds_alternative<api::Executing>(status)) {
			if (std::holds_alternative<api::Parsing>(status)
			    || std::holds_alternative<api::TypeAnalysis>(status)) {
				return cpp::fail(api::CoreOperationError{ api::OtherError{
					"Cannot do memory request while parsing or analyzing types" } });
			}
		}
		api::ExecStatus execStatus = std::get<api::Executing>(status).exec_status;
		if (std::holds_alternative<api::Running>(execStatus))
			return cpp::fail(api::CoreOperationError{
				api::OtherError{ "Cannot do memory request while program is running" } });
		cpp::result<api::Response, api::CoreOperationError> response;
		variant_match(request) {
			variant_case(api::request::TypeMetadata, type_request) {
				auto res
					= type_meta_data.getTypeByName(base::StrID(type_request.type_name.c_str()));
				match_optional(res) {
					opt_some(value) { response = value; }
					opt_none {
						response = cpp::failure(api::CoreOperationError{
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

	cpp::result<api::Response, api::CoreOperationError>
		VMProcess::doRequest(const api::IORequest& request) {
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

	cpp::result<api::Response, api::CoreOperationError>
		VMProcess::doRequest(const api::RequestVariant& request) {
		variant_match(request) {
			variant_case(api::ExecutorRequest, exec_request) { return doRequest(exec_request); }
			variant_case(api::DataRequest, data_request) { return doRequest(data_request); }
			variant_case(api::IORequest, io_request) { return doRequest(io_request); }
			variant_case(api::StatusRequest, status_request) { return api::Response(getStatus()); }
			variant_default { return api::Response(api::response::Empty()); }
		}
		CORE_UNREACHABLE();
	}

	VMProcess::VMProcess(): status(api::ExecutionNotStarted{}), preprocessor(*this) {
		vm_threads.emplace_back(*this);
	}

	VMProcess::~VMProcess() {
		for (auto& t: vm_threads)
			if (t.exec_thread) (void) (stop());
	}

	ProcIO& VMProcess::getIO() { return io; }

	VMThread& VMProcess::getMainVMThread() { return vm_threads.front(); }

	cpp::result<api::Response, api::CoreOperationError>
		VMProcess::attach(std::istream& istream, std::ostream& ostream) {
		// @TODO: Flush the ostream from ProcIO to new ostream.
		if (io_redirecter) return cpp::failure(api::CoreOperationError{ api::AttachDetachError{} });

		// So long this object lives, any IO is redirected.
		io_redirecter.emplace(io.attach(istream, ostream));
		return api::Response(api::response::Empty());
	}

	cpp::result<api::Response, api::CoreOperationError> VMProcess::detach() {
		if (!io_redirecter)
			return cpp::failure(api::CoreOperationError{ api::AttachDetachError{} });
		io_redirecter.reset();
		return api::Response(api::response::Empty());
	}

	bool VMProcess::waitForPaused() {
		std::shared_lock lock(rwStatus);
		status_cv.wait(lock, [&] {
			if (!std::holds_alternative<api::Executing>(status))
				return true;  // If not executing return true
			auto exec_status = std::get<api::Executing>(status).exec_status;
			return std::holds_alternative<api::Terminated>(exec_status)
			    || std::holds_alternative<api::PausedOnError>(exec_status)
			    || std::holds_alternative<api::Panicked>(exec_status)
			    || std::holds_alternative<api::Paused>(exec_status);
		});
		if (std::holds_alternative<api::Executing>(status)) {
			auto exec_status = std::get<api::Executing>(status).exec_status;
			if (std::holds_alternative<api::Paused>(exec_status)) return true;
		}
		return false;
	}

	static void printVariant(api::ProcStatus procStatus) {
		variant_match(procStatus) {
			variant_case_novalue(api::ExecutionNotStarted) { std::cout << "ExecutionNotStarted"; }
			variant_case_novalue(api::Parsing) { std::cout << "Parsing"; }
			variant_case_novalue(api::TypeAnalysis) { std::cout << "TypeAnalysis"; }
			variant_case(api::Panicked, panicked) {
				std::cout << "Panicked: " << panicked.exception.what();
			}
			variant_case_novalue(api::Executing) { std::cout << "Executing"; }
		}
		if (std::holds_alternative<api::Executing>(procStatus)) {
			auto exec_status = std::get<api::Executing>(procStatus).exec_status;
			variant_match(exec_status) {
				variant_case_novalue(api::Running) { std::cout << "Running"; }
				variant_case_novalue(api::Paused) { std::cout << "Paused"; }
				variant_case(api::PausedOnError, pausedOnError) {
					std::cout << "PausedOnError: " << pausedOnError.reason;
				}
				variant_case_novalue(api::WaitingForInput) { std::cout << "WaitingForInput"; }
				variant_case_novalue(api::NotStarted) { std::cout << "NotStarted"; }
				variant_case_novalue(api::Terminated) { std::cout << "Terminated"; }
			}
		}
		std::cout << std::endl;
	}

	/**
	 * @brief Waits for "api::Running" status or for end of execution.
	 * It doesn't work in the current concurrency model, 
	 * because the status can be changed twice before the thread is notified.
	 * @return true the status is "api::Running"
	 * @return false the thread ended execution
	 */
	bool VMProcess::waitForResumed() {
		std::shared_lock lock(rwStatus);
		status_cv.wait(lock, [&] {
			std::cout << "checking wait condition" << std::endl;
			printVariant(status);
			if (!std::holds_alternative<api::Executing>(status))
				return true;  // If not executing return true
			auto exec_status = std::get<api::Executing>(status).exec_status;
			return std::holds_alternative<api::Terminated>(exec_status)
			    || std::holds_alternative<api::PausedOnError>(exec_status)
			    || std::holds_alternative<api::Panicked>(exec_status)
			    || std::holds_alternative<api::Running>(exec_status);
		});
		if (std::holds_alternative<api::Executing>(status)) {
			auto exec_status = std::get<api::Executing>(status).exec_status;
			if (std::holds_alternative<api::Running>(exec_status)) return true;
		}
		return false;
	}
}

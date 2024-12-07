#include "vmprocess.hpp"
#include "base/exceptions.hpp"
#include "services_data/memory/memory.hpp"
#include <services/preprocessor_f8/preprocessor.hpp>
#include <core/thread/vmthread.hpp>

#include <memory>
#include <mutex>
#include <base/variant.hpp>
#include <api/data/request.hpp>

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
		auto code_result = service_manager.get<vm::Preprocessor>().getCode(path);

		if (code_result.has_value()) {
			loadedCode = base::Optional(code_result.value());
			return api::Response(api::response::Empty());
		} else {
			return cpp::failure(api::LoadProgramError{ code_result.error() });
		}
	}

	cpp::result<api::Response, api::CoreOperationError> VMProcess::run() {
		std::unique_lock lock(rwGlobal);
		if (coreThread)
			return cpp::failure(api::CoreOperationError{ api::RunError{} }
			);  // @TODO: change to more verbose error handling
		if (!loadedCode.has_value())
			return cpp::failure(api::CoreOperationError{ api::RunError{} });
		service_manager.get<vm::VMThread>().prestart();
		coreThread = std::make_unique<std::thread>([this] {
			try {
				service_manager.get<vm::VMThread>().run(*loadedCode);

				// @TODO: catch not general std::exception&
			} catch (const std::exception& e) {
				std::cerr << "VCPU PANICKED WITH: " << e.what() << "\n";
				onEvent(api::ProcStatus{ api::Panicked{ e } });
			}
		});
		return api::Response(api::response::Empty());
	}

	cpp::result<api::Response, api::CoreOperationError> VMProcess::join() {
		// @TODO: more verbose errors
		// @TODO: check status
		if (coreThread && coreThread->joinable())
			coreThread->join();
		else
			return cpp::failure(api::CoreOperationError{ api::JoinError{} });
		return api::Response(api::response::Empty());
	}

	cpp::result<api::Response, api::CoreOperationError>
		VMProcess::input(const api::request::Input& /*request*/) {
		throw base::NotYetImplemented("Input from api not yet implemented");
		// TODO: add checking for stdio
		// std::unique_lock lock(input_mutex);
		// input_stream->write(request.input.c_str(), std::streamsize(request.input.size()));
		// service_manager.get<vm::VMThread>().notifyPaused();
		// return api::Response(api::response::Empty());
	}

	cpp::result<api::Response, api::CoreOperationError> VMProcess::output() {
		throw base::NotYetImplemented("Output from api not yet implemented");
		// TODO: add checking for stdio
		// std::unique_lock lock(output_mutex);
		// std::string      content;
		// output_empty_cv.wait(lock, [&] { return !(content = output_stream->str()).empty(); });
		// output_stream->str(std::string());
		// return api::Response(api::response::Output{ content });
	}

	cpp::result<api::Response, api::CoreOperationError> VMProcess::stop() {
		service_manager.get<vm::VMThread>().stop();
		(void) join();
		coreThread.reset(nullptr);
		return api::response::Empty{};
	}

	cpp::result<api::Response, api::CoreOperationError>
		VMProcess::doRequest(const api::ExecutorRequest& request) {
		variant_match(request) {
			variant_case_novalue(api::request::Run) { return run(); }
			variant_case_novalue(api::request::Join) { return join(); }
			variant_case_novalue(api::request::Pause) {
				service_manager.get<vm::VMThread>().pause();
				return api::Response(api::response::Empty());
			}
			variant_case_novalue(api::request::Resume) {
				service_manager.get<vm::VMThread>().resume();
				return api::Response(api::response::Empty());
			}
			variant_case_novalue(api::request::Step) {
				service_manager.get<vm::VMThread>().step();
				return api::Response(api::response::Empty());
			}
			variant_case(api::request::Load, load_request) {
				return loadProgram(load_request.filename).map_error([](auto err) {
					return api::CoreOperationError{ err };
				});
			}
			variant_case_novalue(api::request::Stop) { return stop(); }
			variant_case(api::request::Input, input_request) { return input(input_request); }
			variant_case_novalue(api::request::Output) { return output(); }
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
					memory.getBlock(block_request.block_id)->rawPointer() });
			}
			variant_default { response = api::Response(api::response::Empty()); }
		}
		return response;
	}

	cpp::result<api::Response, api::CoreOperationError>
		VMProcess::doRequest(const api::RequestVariant& request) {
		variant_match(request) {
			variant_case(api::ExecutorRequest, exec_request) { return doRequest(exec_request); }
			variant_case(api::DataRequest, data_request) { return doRequest(data_request); }
			variant_case(api::StatusRequest, status_request) { return api::Response(getStatus()); }
			variant_default { return api::Response(api::response::Empty()); }
		}
		CORE_UNREACHABLE();
	}

	VMProcess::~VMProcess() {
		if (coreThread) (void) (stop());
	}

	ProcIO& VMProcess::getIO() { return io; }
}

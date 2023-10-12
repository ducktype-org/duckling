#include <mutex>
#include <base/variant.hpp>
#include "vcpu.hpp"

namespace vm {
	DataManager& VCPU::getData() { return dataManager; }

	ServiceManager& VCPU::getServices() { return serviceManager; }

	void VCPU::onEvent(const api::VCPUStatus& event) noexcept {
		std::unique_lock lock(rwStatus);
		// @TODO: check if status change is legal
		status = event;
		status_cv.notify_all();
	}

	api::VCPUStatus VCPU::getStatus() {
		std::shared_lock lock(rwStatus);
		return status;
	}

	result<api::Response, api::LoadProgramError> VCPU::loadProgram(const fs::FilePath& path) {
		std::unique_lock lock(rwGlobal);
		// @TODO: this code should be improved in the future to not just return plain strings
		auto code_result = serviceManager.get<vm::Preprocessor>().getCode(path);

		if (code_result.has_value()) {
			loadedCode = code_result.value();
			return api::Response(api::response::Empty());
		} else {
			return failure(api::LoadProgramError(code_result.error()));
		}
	}

	result<api::Response, api::CoreOperationError> VCPU::run() {
		std::unique_lock lock(rwGlobal);
		if (coreThread)
			return failure(api::RunError{});  // @TODO: change to more verbose error handling
		if (!loadedCode.has_value()) return failure(api::RunError{});
		// @TODO: check VCPU status for loaded code
		if (!uses_stdio) {
			input_stream  = base::make_unique<std::stringstream>(std::stringstream());
			output_stream = base::make_unique<std::stringstream>(std::stringstream());
		}
		serviceManager.get<vm::Executor>().prestart();
		coreThread.reset(new std::thread([this] {
			try {
				serviceManager.get<vm::Executor>().run(*loadedCode);

				// @TODO: catch not general std::exception&
			} catch (const std::exception& e) {
				std::cerr << "VCPU PANICKED WITH: " << e.what() << "\n";
				onEvent(api::VCPUStatus{ api::Panicked(e) });
			}
		}));
		return api::Response(api::response::Empty());
	}

	result<api::Response, api::CoreOperationError> VCPU::join() {
		// @TODO: more verbose errors
		// @TODO: check status
		if (coreThread && coreThread->joinable())
			coreThread->join();
		else
			return failure(api::JoinError{});
		return api::Response(api::response::Empty());
	}

	result<api::Response, api::CoreOperationError> VCPU::input(const api::request::Input& request) {
		// TODO: add checking for stdio
		std::unique_lock lock(input_mutex);
		input_stream->write(request.input.c_str(), request.input.size());
		serviceManager.get<vm::Executor>().notifyPaused();
		return api::Response(api::response::Empty());
	}

	result<api::Response, api::CoreOperationError> VCPU::output() {
		// TODO: add checking for stdio
		std::unique_lock lock(output_mutex);
		std::string      content = output_stream->str();
		output_stream->str(std::string());
		return api::Response(api::response::Output{ content });
	}

	result<api::Response, api::CoreOperationError> VCPU::stop() {
		serviceManager.get<vm::Executor>().stop();
		ignore(join());
		coreThread.reset(nullptr);
		return api::response::Empty{};
	}

	result<api::Response, api::CoreOperationError>
		VCPU::doRequest(const api::ExecutorRequest& request) {
		variant_match(request) {
			variant_case_novalue(api::request::Run) { return run(); }
			variant_case_novalue(api::request::Join) { return join(); }
			variant_case_novalue(api::request::Pause) {
				serviceManager.get<vm::Executor>().pause();
				return api::Response(api::response::Empty());
			}
			variant_case_novalue(api::request::Resume) {
				serviceManager.get<vm::Executor>().resume();
				return api::Response(api::response::Empty());
			}
			variant_case_novalue(api::request::Step) {
				serviceManager.get<vm::Executor>().step();
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
		RIFT_PANIC("something went wrong");
	}

	result<api::Response, api::CoreOperationError> VCPU::doRequest(const api::DataRequest& request
	) {
		std::shared_lock lock(rwStatus);
		if (!std::holds_alternative<api::Executing>(status)) {
			if (std::holds_alternative<api::Parsing>(status)
			    || std::holds_alternative<api::TypeAnalysis>(status)) {
				return fail(
					api::OtherError("Cannot do memory request while parsing or analyzing types")
				);
			}
		}
		api::ExecStatus execStatus = std::get<api::Executing>(status).exec_status;
		if (std::holds_alternative<api::Running>(execStatus))
			return fail(api::OtherError("Cannot do memory request while program is running"));
		result<api::Response, api::CoreOperationError> response;
		variant_match(request) {
			variant_case(api::request::TypeMetadata, type_request) {
				response
					= dataManager.get<vm::TypeMetadata>()
				          .getTypeByName(base::StrId(type_request.type_name.c_str()))
				          .map([](const TypeCRef& type_ptr) { return api::Response(type_ptr); })
				          .map_error([](auto&) {
							  return api::CoreOperationError{ api::OtherError("Type not found") };
						  });
			}
			variant_case(api::request::Block, block_request) {
				response
					= dataManager.get<vm::Memory>()
				          .getBlock(block_request.block_id)
				          .map([](Block* block_ptr) {
							  return api::Response(api::response::Block{ block_ptr->rawPointer() });
						  })
				          .map_error([](auto& error) {
							  return api::CoreOperationError{ api::OtherError(error) };
						  });
			}
			variant_default { response = api::Response(api::response::Empty()); }
		}
		return response;
	}

	result<api::Response, api::CoreOperationError>
		VCPU::doRequest(const api::RequestVariant& request) {
		variant_match(request) {
			variant_case(api::ExecutorRequest, exec_request) { return doRequest(exec_request); }
			variant_case(api::DataRequest, data_request) { return doRequest(data_request); }
			variant_default { return api::Response(getStatus()); }
		}
		RIFT_PANIC("something went wrong");
	}

	VCPU::~VCPU() {
		if (coreThread) ignore(stop());
	}
}

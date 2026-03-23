#include "vmprocess.hpp"

#include <base/extend_cpp/variant_match.hpp>
#include <vm/api/data/response.hpp>

namespace vm {

	VMProcess::VMProcess(const PID my_pid):
		  my_pid(my_pid) {}

	ProcIO& VMProcess::getIO() { return io; }

	GIL& VMProcess::getGIL() { return gil; }

	SynchronizationPrimitives& VMProcess::getSynchronizationPrimitives() {
		return synchronization_primitives;
	}

	PID VMProcess::getPID() const { return my_pid; }

	std::expected<api::Response, api::ApiError> VMProcess::attach(
std::istream& istream, std::ostream& ostream
	) {
		if (io_redirecter) return std::unexpected(api::ApiError{ api::AttachDetachError{} });
		io_redirecter.emplace(io.attach(istream, ostream));
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> VMProcess::detach() {
		if (!io_redirecter) return std::unexpected(api::ApiError{ api::AttachDetachError{} });
		io_redirecter.reset();
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> VMProcess::doRequest(
const api::RequestVariant& request
	) {
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
				auto response = pauseVMThread(pause_request.thread_id);
				if (response) return std::unexpected(*response);
				return getVMThreadCurrentPosition(pause_request.thread_id);
			}

			variant_case(api::request::Resume, resume_request) {
				auto response = resumeVMThread(resume_request.thread_id);
				if (response) return std::unexpected(*response);
				return api::Response(api::response::Empty());
			}

			variant_case_novalue(api::request::Step) {
				auto response = stepMainVMThread();
				if (response) return std::unexpected(*response);
				return getMainVMThreadCurrentPosition();
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
				return getMainVMThreadCurrentPosition();
			}

			variant_case_novalue(api::request::WaitForBreakpoint) {
				waitForBreakpoint();
				api::ProcStatus stat = getStatus();
				if (!std::holds_alternative<api::Paused>(stat))
					return std::unexpected(api::ApiError{
api::OtherError{ "unexpected status response" } });

				return getMainVMThreadCurrentPosition();
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
				return getTypeMetadata(vmvalue_request.type_name);
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
}

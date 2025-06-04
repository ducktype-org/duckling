#include "request.hpp"

namespace vm::api {
	SupervisorRequest makeExecutorRequest(PID pid, ExecutorRequest&& data) {
		return SupervisorRequest{ .pid = pid, .request = std::move(data) };
	}

	SupervisorRequest makeDataRequest(PID pid, DataRequest&& data) {
		return SupervisorRequest{ .pid = pid, .request = std::move(data) };
	}

	SupervisorRequest makeStatusRequest(PID pid) {
		return SupervisorRequest{ .pid = pid, .request = StatusRequest{} };
	}

	SupervisorRequest makeExitCodeRequest(PID pid) {
		return SupervisorRequest{ .pid = pid, .request = ExitCodeRequest{} };
	}

	SupervisorRequest makeIORequest(PID pid, IORequest&& data) {
		return SupervisorRequest{ .pid = pid, .request = std::move(data) };
	}

}

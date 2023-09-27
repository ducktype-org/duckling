#include "request.hpp"

namespace vm::api {
	SupervisorRequest makeExecutorRequest(PID pid, ExecutorRequest &&data) {
		return SupervisorRequest{ pid, data };
	}

	SupervisorRequest makeDataRequest(PID pid, DataRequest &&data) {
		return SupervisorRequest{ pid, data };
	}

	SupervisorRequest makeStatusRequest(PID pid) {
		return SupervisorRequest{ pid, StatusRequest{} };
	}
}

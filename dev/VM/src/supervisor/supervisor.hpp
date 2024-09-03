#pragma once

#include <mutex>
#include <string>
#include <base/optional.hpp>
#include <listener/listener.hpp>
#include <filesystem/file.hpp>
#include "vcpu.hpp"
#include <api/api.hpp>
#include <base/smart_pointers.hpp>

namespace vm {
	class Supervisor {
	private:
		Supervisor() = default;

		std::shared_mutex                               rwProcessTable;
		PID                                             next = 0;
		std::unordered_map<PID, base::unique_ptr<VCPU>> processTable;

		cpp::result<base::borrow_ptr<VCPU>, api::ApiError> getProcess(PID pid);

	public:
		static Supervisor& get();

		// Each of the following methods should synchronize access to the processTable, but should
		// not synchronize usage of each of the processes. Each process synchronizes its resources
		// by itself
		cpp::result<PID, api::ApiError>           newProcess(bool usesStdio);
		cpp::result<api::Response, api::ApiError> doRequest(const api::SupervisorRequest& request);
		cpp::result<void, api::ApiError>          killProcess(PID pid);
	};
}

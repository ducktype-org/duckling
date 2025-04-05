#pragma once

#include <filesystem/file.hpp>
#include <listener/listener.hpp>

#include <base/optional.hpp>

#include <vm/api/api.hpp>

namespace vm {
	class VMProcess;

	class Supervisor final {
	private:
		Supervisor() = default;

		std::shared_mutex                       rw_process_table;
		PID                                     next = 0;
		std::unordered_map<PID, Box<VMProcess>> process_table;

		cpp::result<Ref<VMProcess>, api::ApiError> getProcess(PID pid);


	public:
		static Supervisor& get();

		// Each of the following methods should synchronize access to the processTable, but should
		// not synchronize usage of each of the processes. Each process synchronizes its resources
		// by itself
		cpp::result<PID, api::ApiError>           newProcess();
		cpp::result<api::Response, api::ApiError> doRequest(const api::SupervisorRequest& request);
		cpp::result<void, api::ApiError>          killProcess(PID pid);
	};
}

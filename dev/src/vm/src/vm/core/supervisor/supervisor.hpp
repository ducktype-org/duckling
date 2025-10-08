#pragma once

#include <vm/core/process/vmprocess.hpp>

#include <shared_mutex>
#include <unordered_map>

namespace vm {

	class Supervisor final {
	private:
		Supervisor() = default;
		~Supervisor();

		std::shared_mutex                       rw_process_table;
		PID                                     next = 0;
		std::unordered_map<PID, Box<VMProcess>> process_table;

		std::expected<Ref<VMProcess>, api::ApiError> getProcess(PID pid);


	public:
		static Supervisor& get();

		// Each of the following methods should synchronize access to the processTable, but should
		// not synchronize usage of each of the processes. Each process synchronizes its resources
		// by itself
		std::expected<PID, api::ApiError>                    newProcess();
		std::expected<api::response::Boolean, api::ApiError> deinitAndValidate(PID pid);
		std::expected<api::Response, api::ApiError> doRequest(const api::SupervisorRequest& request);
		std::expected<void, api::ApiError> killProcess(PID pid);
	};
}

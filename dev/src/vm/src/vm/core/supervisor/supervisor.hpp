#pragma once

#include <base/collections/maps.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <vm/api/api.hpp>

#include <expected>
#include <shared_mutex>

namespace vm {
	class IVMProcess;

	class Supervisor final {
	private:
		Supervisor() = default;
		~Supervisor();

		std::shared_mutex                   rw_process_table;
		u64                                 next_pid = 0;
		base::HashMap<PID, Box<IVMProcess>> process_table;

		std::expected<Ref<IVMProcess>, api::ApiError> getProcess(PID pid);


	public:
		static Supervisor& get();

		// Each of the following methods should synchronize access to the processTable, but should
		// not synchronize usage of each of the processes. Each process synchronizes its resources
		// by itself
		std::expected<PID, api::ApiError> newProcess(const api::ProcessConfig& options = {});
		std::expected<api::response::Boolean, api::ApiError> deinitAndValidate(PID pid);
		std::expected<api::Response, api::ApiError> doRequest(const api::SupervisorRequest& request);
		std::expected<void, api::ApiError> killProcess(PID pid);
	};
}

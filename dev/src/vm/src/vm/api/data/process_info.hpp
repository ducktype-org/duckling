#pragma once

#include <base/types/ints.hpp>

#include <json/json.hpp>

namespace vm {
	using PID = u32;

	namespace api {
		struct ProcessInfo {
			PID pid;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProcessInfo, pid);
		};

		/**
		 * @brief ProcessConfig serves as configuration which has to be initialized when spawning
		 * the process and can't be changed once the process is spawned.
		 */
		struct ProcessConfig {
			/// Enables FatBytecode to MicroBytecode mapping
			bool enable_mapping = false;
		};
	}
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::ProcessInfo, "ProcessInfo")

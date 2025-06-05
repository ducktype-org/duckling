#pragma once

#include <json/json.hpp>

#include <base/ints.hpp>

namespace vm {
	using PID = u32;

	namespace api {
		struct ProcessInfo {
			PID pid;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProcessInfo, pid);
		};
	}
}

JSON_REGISTER_TYPE_WITH_NAME(vm::api::ProcessInfo, "ProcessInfo")

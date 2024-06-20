#pragma once

#include <base/ints.hpp>
#include <json/json.hpp>

namespace vm {
	using PID = u32;

	namespace api {
		struct ProcessInfo {
			PID pid;

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProcessInfo, pid);
		};
	}
}

REGISTER_PARSE_TYPE_ALIAS(vm::api::ProcessInfo, "ProcessInfo")

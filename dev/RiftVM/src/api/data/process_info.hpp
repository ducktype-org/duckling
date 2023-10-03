#pragma once

#include <base/ints.hpp>
#include <json/json.hpp>

namespace vm {
	using PID = u32;

	namespace api {
		struct ProcessInfo {
			PID pid;

			JS_OBJ(pid);
		};
	}  // namespace api
}  // namespace vm

REGISTER_PARSE_TYPE_ALIAS(vm::api::ProcessInfo, "ProcessInfo")

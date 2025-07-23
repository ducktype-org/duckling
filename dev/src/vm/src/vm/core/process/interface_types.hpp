#pragma once
#include <vm/core/thread/vmvalue.hpp>
#include <vm/services/service_manager.hpp>

namespace vm {
	using ServiceManager       = ServiceManagerDef<ReferenceCounter, Profiler>;
	using ProgramRunArguments  = std::vector<std::string>;
	using FunctionRunArguments = std::vector<CRef<VmValue>>;
	using RunArguments         = std::variant<ProgramRunArguments, FunctionRunArguments>;
}

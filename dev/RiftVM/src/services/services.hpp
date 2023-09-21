#pragma once

// The order here is very important. Under no circumstances change it!!!
namespace vm {
	template<class... DynamicServices>
	class ServiceManagerDef;
}

#include "allocator/allocator.hpp"
#include "allocator/stack_allocator.hpp"
#include "executor_f8/executor.hpp"
#include "preprocessor_f8/preprocessor.hpp"

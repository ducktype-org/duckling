#pragma once

#include <query_framework/query_int.hpp>
#include <query_framework/query_int.hpp>

namespace compiler::mir {
	struct Function;

	Function addDestructors(query::Context&, Function);
}

#pragma once

#include <query_framework/query_int.hpp>
#include <filesystem/file.hpp>
#include "module_tree.hpp"

namespace compiler::frontend {

	DECLARE_QUERY(GetModuleTreeQuery, fs::FilePath, ModuleId)

}

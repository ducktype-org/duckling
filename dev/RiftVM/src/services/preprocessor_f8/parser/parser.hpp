#pragma once

#include <base/option.hpp>
#include <code_data/code.hpp>
#include <filesystem/file.hpp>
#include <services_data/type_metadata/type_metadata.hpp>
#include <string>

namespace assemble {
	result<vm::Code, std::string> assemble(fs::FilePath file, vm::TypeMetadata &type_metadata);
}

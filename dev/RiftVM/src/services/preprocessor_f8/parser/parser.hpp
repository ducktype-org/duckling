#pragma once

#include <code_data/code.hpp>
#include <filesystem/file.hpp>
#include <string>
#include <base/optional.hpp>
#include <services_data/type_metadata/type_metadata.hpp>

namespace assemble {
	cpp::result<vm::Code, std::string> assemble(fs::FilePath file, vm::TypeMetadata& type_metadata);
}

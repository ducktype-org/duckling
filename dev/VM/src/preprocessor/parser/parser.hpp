#pragma once

#include <code_data/code.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>
#include <core/process/type_metadata/type_metadata.hpp>

namespace assemble {
	cpp::result<vm::Code, std::string>
		assemble(const fs::FilePath& file, vm::TypeMetadata& type_metadata);
}

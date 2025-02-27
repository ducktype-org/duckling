#pragma once

#include <code_data/code.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>
#include <core/process/type_metadata/type_metadata.hpp>

namespace assemble {
	std::expected<vm::VMProgram, std::string>
		assemble(const std::vector<fs::FilePath>& files);
		
	std::expected<Box<vm::VMProgram>, std::string> convertParsedProgramToVMProgram(CBox<ParsedProgram> parsed_program);
}

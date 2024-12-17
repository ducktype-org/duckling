#pragma once

#include <program/program.hpp>
#include <filesystem/file.hpp>
#include <string>
#include <base/optional.hpp>
#include <services_data/type_metadata/type_metadata.hpp>

namespace vm {
	class VMProgram;
}

namespace assemble {
	// struct ParsedCode;

	cpp::result<vm::VMProgram, std::string> assemble(const fs::FilePath& file);
	cpp::result<vm::VMProgram, std::string> assemble(const std::vector<fs::FilePath>& file);
	// cpp::result<vm::VMProgram, std::string> assemble(const std::vector<ParsedCode>& codes);
	// cpp::result<ParsedCode, std::string>    parse(const fs::FilePath& file);
}

#include "preprocessor.hpp"
#include "program/program.hpp"
#include <base/exceptions.hpp>
#include <core/process/vmprocess.hpp>

namespace vm {
	cpp::result<vm::VMProgram, std::string> Preprocessor::getCode(const fs::FilePath& file) {
		return vm::VMProgram::assemble(file);
	}

	cpp::result<vm::VMProgram, std::string>
		Preprocessor::getCode(const std::vector<fs::FilePath>& files) {
		return vm::VMProgram::assemble(files);
	}
	Preprocessor::Preprocessor(VMProcess& process): type_metadata(process.getTypeMetadata()) {}
}

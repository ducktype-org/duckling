#include "preprocessor.hpp"
#include <core/process/vmprocess.hpp>
#include "parser/parser.hpp"

namespace vm {
	std::expected<vm::Code, std::string> Preprocessor::getCode(const fs::FilePath& file) {
		return assemble::assemble(file, type_metadata);
	}

	Preprocessor::Preprocessor(VMProcess& process): type_metadata(process.getTypeMetadata()) {}
}

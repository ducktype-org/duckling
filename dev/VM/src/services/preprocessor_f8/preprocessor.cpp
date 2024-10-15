#include "preprocessor.hpp"
#include "parser/parser.hpp"

namespace vm {
	cpp::result<vm::Code, std::string> Preprocessor::getCode(const fs::FilePath& file) {
		return assemble::assemble(file, type_metadata);
	}
}

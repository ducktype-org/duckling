#include "preprocessor.hpp"

#include "parser/parser.hpp"

namespace vm {
	result<vm::Code, std::string> Preprocessor::getCode(const fs::FilePath& file) {
		return assemble::assemble(file, type_metadata);
	}
}  // namespace vm

#include "preprocessor.hpp"
#include "base/exceptions.hpp"
#include "parser/parser.hpp"

namespace vm {
	cpp::result<vm::VMProgram, std::string> Preprocessor::getCode(const fs::FilePath& file) {
		return assemble::assemble(file, type_metadata);
	}

	cpp::result<vm::VMProgram, std::string>
		Preprocessor::getCode(const std::vector<fs::FilePath>& files) {
		// @TODO: Load files separately and then merge.
		throw base::NotYetImplemented("Can\'t load multiple files yet...");
	}
}

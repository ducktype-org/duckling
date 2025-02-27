#include "preprocessor.hpp"
#include <core/process/vmprocess.hpp>
#include "parser/parser.hpp"
#include "validator/validator.hpp"

namespace vm {
	std::expected<vm::VMProgram, std::string> Preprocessor::getProgram(const fs::FilePath& file) {
		return getProgram(std::vector{ file });
	}

	std::expected<vm::VMProgram, std::string> Preprocessor::getProgram(const std::vector<fs::FilePath>& files) {
		auto parsedProgram = assemble::assemble(files);

		bool is_valid = false;
		if (validate_program) {
			// is_valid = validator.validateProgram(parsedProgram);
		}
		// auto program = changeParsedProgramToVMProgram(parsedProgram);

		return parsedProgram;
	}

	std::expected<vm::VMProgram, std::string> Preprocessor::changeParsedProgramToVMProgram(const assemble::ParsedProgram& parsed_program) {
		assemble::
	}

	

	Preprocessor::Preprocessor(VMProcess& process, bool validate_program): validate_program(validate_program) {}
}

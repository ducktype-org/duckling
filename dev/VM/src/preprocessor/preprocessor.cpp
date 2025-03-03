#include "preprocessor.hpp"
#include <core/process/vmprocess.hpp>
#include "parser/parser.hpp"
#include "validator/validator.hpp"

namespace vm {
	std::expected<vm::VMProgram, std::string> Preprocessor::getProgram(const fs::FilePath& file) {
		return getProgram(std::vector{ file });
	}

	std::expected<vm::VMProgram, std::string>
		Preprocessor::getProgram(const std::vector<fs::FilePath>& files) {
		auto log                  = dia::Logger();
		auto maybe_parsed_program = assemble::assemble(files, log);
		if (auto parsed_program = std::move(maybe_parsed_program).toOptBox(); parsed_program) {
			bool is_valid = false;
			if (validate_program) {
				// is_valid = validator.validateProgram(parsed_program);
			}

			auto program = assemble::changeParsedProgramToVMProgram(parsed_program->refMut(), log);


			if (!program) {
				std::stringstream stream;
				log.dumpLogAndClear(true, stream);
				return std::unexpected(stream.str());
			}

			// return std::expected<vm::VMProgram, std::string(program->);
			return std::move(*program->refMut());

		} else {
			std::stringstream stream;
			log.dumpLogAndClear(true, stream);
			return std::unexpected(stream.str());
		}
	}

	Preprocessor::Preprocessor(VMProcess& process, bool validate_program):
		  validate_program(validate_program) {}
}

#include "validator.hpp"
#include "errors.hpp"
#include <expected>
#include <sstream>

namespace vm::validator {
	std::expected<bool, std::string> Validator::validateProgram(const parser::ParsedProgram& program
	) {
		auto log   = dia::Logger();
		bool valid = false;

		valid = validateMainExistance(program, log);
		if (!valid) {
			std::stringstream stream;
			log.dumpLogAndClear(true, stream);
			return std::unexpected(stream.str());
		}
		valid = validateTailcallSignatures(program, log);
		if (!valid) {
			std::stringstream stream;
			log.dumpLogAndClear(true, stream);
			return std::unexpected(stream.str());
		}
		valid = validateDuplicateFunctionDeclarations(program, log);
		if (!valid) {
			std::stringstream stream;
			log.dumpLogAndClear(true, stream);
			return std::unexpected(stream.str());
		}
		return true;
	}

	bool Validator::validateMainExistance(const parser::ParsedProgram& program, dia::Logger& log) {
		bool main_found = false;

		for (auto& func: program.functions) {
			if (func->name.value.strView() == "main") {
				main_found = true;
				break;
			}
		}

		if (!main_found) {
			// @TODO: Change fakePosition() to real source position.
			log.log(makeBox<vm::validator::NoMainError>(dia::SourcePosition::fakePosition()));
			return false;
		}
		return true;
	}

	bool Validator::validateTailcallSignatures(
		const parser::ParsedProgram& program, dia::Logger& log
	) {
		return true;
	}

	bool Validator::validateDuplicateFunctionDeclarations(
		const parser::ParsedProgram& program, dia::Logger& log
	) {
		return true;
	}


}

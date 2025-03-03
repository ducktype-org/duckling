#include "validator.hpp"
#include <iostream>
#include "errors.hpp"

namespace vm::validator {
	bool vm::validator::Validator::validateProgram(const assemble::ParsedProgram& program) {
		auto log = dia::Logger();
		bool bad = false;

		bad = validateMainExistance(program, log);
		if (bad) return false;
		bad = validateTailcallSignatures(program, log);
		if (bad) return false;
		return true;
	}

	bool validateMainExistance(const assemble::ParsedProgram& program, dia::Logger& log) {
		base::Optional<usize> main_id;
		for (usize idx = 0; idx < program.functions.size(); idx++) {
			if (program.functions[idx]->name.value.strView() == "main") {
				main_id = idx;
				break;
			}
		}

		if (!main_id) {
			log.log(makeBox<vm::validator::NoMainError>(dia::SourcePosition::fakePosition()));
			return false;
		}
		return true;
	}

	bool validateTailcallSignatures(const assemble::ParsedProgram& program, dia::Logger& log) {
		// @TODO: Move tailcall checking from parser to here.
		return true;
	}


}

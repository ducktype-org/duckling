#pragma once

#include <code_data/program.hpp>
#include "../parser/parser.hpp"

namespace vm::validator {
	class Validator {
	public:
		Validator() = default;

		bool validateProgram(const assemble::ParsedProgram& program);

	private:
		vm::VMProgram program;


		bool validateMainExistance(const assemble::ParsedProgram& program, dia::Logger& log);
		bool validateTailcallSignatures(const assemble::ParsedProgram& program, dia::Logger& log);
	};

}

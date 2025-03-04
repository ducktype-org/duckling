#pragma once

#include "preprocessor/parser/elements.hpp"
#include <code_data/program.hpp>
#include "diagnostic/logger.hpp"

namespace vm::validator {
	class Validator {
	public:
		Validator() = default;

		std::expected<bool, std::string> validateProgram(const parser::ParsedProgram& program);

	private:
		bool validateMainExistance(const parser::ParsedProgram& program, dia::Logger& log);
		bool validateTailcallSignatures(const parser::ParsedProgram& program, dia::Logger& log);
		bool validateDuplicateFunctionDeclarations(
			const parser::ParsedProgram& program, dia::Logger& log
		);
	};

}

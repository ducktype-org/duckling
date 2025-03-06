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
		void preprocessProgram(const parser::ParsedProgram& program, dia::Logger& log);
		void validateMainExistance(const parser::ParsedProgram& program, dia::Logger& log);
		void validateTailcallSignatures(const parser::ParsedProgram& program, dia::Logger& log);
		void validateDuplicateFunctionDeclarations(
			const parser::ParsedProgram& program, dia::Logger& log
		);
	};

}

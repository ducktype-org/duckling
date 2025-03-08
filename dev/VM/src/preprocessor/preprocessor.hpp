#pragma once

#include <core/process/type_metadata/type_metadata.hpp>
#include <code_data/program.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>
#include "parser/parser.hpp"

namespace vm {
	class VMProcess;

	/**
	 * @brief Service that loads the program file to the VM.
	 * Current preprocessor pipeline is as follows:
	 * 1) Parse the program from a given list of files and create
	 *    a ParsedProgram object, enriched in source positions
	 *    of every opcode, type etc.
	 * 2) Perform static verification of the code using the Validator module
	 * 3) Convert ParsedProgram object into VMProgram which will be used to
	 *    execute the code.
	 */
	class Preprocessor {
		friend VMProcess;

	private:
		bool validate_program;

		/**
		 * @brief Changes the program representation from parser representation
		 * (with source positions) to a program format executable by the VM.
		 *
		 * @param parsed_program
		 * @return std::expected<vm::VMProgram, std::string>
		 */
		std::expected<vm::VMProgram, std::string>
			changeParsedProgramToVMProgram(const parser::ParsedProgram& parsed_program);

		Preprocessor(VMProcess& process, bool validate_program);

	public:
		template<class... DynamicServices>
		friend class ServiceManagerDef;

		/**
		 * @brief Parses the file, returns the representation of the program with type metadata.
		 *
		 * @param file
		 * @return std::expected<vm::VMProgram, std::string>
		 */
		std::expected<vm::VMProgram, std::string> getProgram(const fs::FilePath& file);


		/**
		 * @brief Parses a list of files, returns the representation of the program with type
		 * metadata.
		 *
		 * @param files
		 * @return std::expected<vm::VMProgram, std::string>
		 */
		std::expected<vm::VMProgram, std::string> getProgram(const std::vector<fs::FilePath>& files
		);
	};
}

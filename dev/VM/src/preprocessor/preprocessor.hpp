#pragma once

#include <program/program.hpp>
#include <core/process/type_metadata/type_metadata.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>

namespace vm {
	class VMProcess;

	// @TODO: static type checking

	/**
	 * @brief Service that loads the program file to the VM.
	 * @TODO: Refactor this to return a program object....
	 */
	class Preprocessor {
		friend VMProcess;

	private:
		TypeMetadata& type_metadata;

		Preprocessor(VMProcess& process);

	public:
		template<class... DynamicServices>
		friend class ServiceManagerDef;

		/**
		 * @brief Parses the file, creates type metadata and returns the code.
		 *
		 * @param file
		 * @return cpp::result<vm::Code, std::string>
		 */
		cpp::result<vm::VMProgram, std::string> getCode(const fs::FilePath& file);

		cpp::result<vm::VMProgram, std::string> getCode(const std::vector<fs::FilePath>& files);
	};
}

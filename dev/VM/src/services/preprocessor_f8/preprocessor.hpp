#pragma once

#include "../services.hpp"
#include <code_data/code.hpp>
#include <filesystem/file.hpp>
#include <base/optional.hpp>

namespace vm {
	// @TODO: static type checking

	/**
	 * @brief Service that loads the program file to the VM.
	 */
	class Preprocessor {
	private:
		TypeMetadata& type_metadata;

		template<class... DynamicServices>
		Preprocessor([[maybe_unused]] ServiceManagerDef<DynamicServices...>& serviceManager):
			  type_metadata(serviceManager.getVCPU().getData().template get<TypeMetadata>()) {}

	public:
		template<class... DynamicServices>
		friend class ServiceManagerDef;

		/**
		 * @brief Parses the file, creates type metadata and returns the code.
		 *
		 * @param file
		 * @return cpp::result<vm::Code, std::string>
		 */
		cpp::result<vm::Code, std::string> getCode(const fs::FilePath& file);
	};
}

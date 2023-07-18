#pragma once

#include "../services.hpp"
#include <code_data/code.hpp>
#include <filesystem/file.hpp>
#include <base/option.hpp>

namespace vm {
	// @TODO: static type checking

	class Preprocessor {
		private:
			TypeMetadata& type_metadata;

			template<class... DynamicServices>
			Preprocessor([[maybe_unused]] ServiceManagerDef<DynamicServices...>& serviceManager):
				type_metadata(serviceManager.getVCPU().getData().template get<TypeMetadata>()) {}
		public:
			
			template<class... DynamicServices>
			friend class ServiceManagerDef;

			result<vm::Code, std::string> getCode(const fs::FilePath& file);
	};
}

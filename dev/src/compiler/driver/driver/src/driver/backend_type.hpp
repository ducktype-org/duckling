#pragma once

#include <archiver/archive.hpp>
#include <linker/link.hpp>

#include <base/types/ints.hpp>

#include <string>
#include <variant>

namespace compiler::driver {
	enum class BackendType : u64 { LLVM, DVM };

	/**
	 * @brief Package build target.
	 */
	struct BuildTargetDVM {};

	struct BuildTargetLLVMExecutable {
		std::string            output_file_name;
		linker::LinkingOptions linking_options;
	};

	struct BuildTargetLLVMStaticLibrary {
		std::string                output_file_name;
		archiver::ArchivingOptions archiving_options;
	};

	using BuildTarget
		= std::variant<BuildTargetDVM, BuildTargetLLVMExecutable, BuildTargetLLVMStaticLibrary>;

	std::string backendTypeToStr(BackendType type);
}

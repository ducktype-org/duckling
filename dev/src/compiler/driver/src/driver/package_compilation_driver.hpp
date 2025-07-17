#pragma once

#include "backend_driver/backend_options.hpp"

#include <artifacts/artifacts.hpp>

namespace compiler::driver {

	/**
	 * Driver that automates process of package compilation,
	 * handles package artifacts and encapsulates some query operations
	 * into high-level cacheable operations.
	 * @note implementation of this class is somewhat temporary and hacky,
	 * it will be changed in to future changes.
	 */
	class PackageCompilationDriver final {
		BackendType                   backend;
		fs::File                      package_location;
		artifacts::ArtifactCollection root_artifact_collection;

	public:
		PackageCompilationDriver()                                = delete;
		PackageCompilationDriver(PackageCompilationDriver&&)      = delete;
		PackageCompilationDriver(const PackageCompilationDriver&) = delete;

		PackageCompilationDriver(
			BackendType backend, fs::File package_location, std::filesystem::path artifact_location
		);

		/**
		 * Compile all package modules and link them into a single binary.
		 */
		void compilerEntirePackageIntoBinary();

	private:
		/**
		 * Compile builtin LLVM library into an object file.
		 */
		artifacts::FileArtifact emitBuiltinObjectFile();
	};
}

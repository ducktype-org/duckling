#pragma once

#include <artifacts/artifacts.hpp>

#include <vector>

namespace compiler::archiver {
	/**
	 * @brief Archiving options used for static library creation.
	 */
	struct ArchivingOptions final {
		/**
		 * @brief Path to the archiver executable (for example, "ar" or "llvm-ar").
		 */
		base::Optional<std::string> archiver_path;
	};

	/**
	 * @brief Archive given object files into a single static library (.a) via `ar rcs`.
	 */
	[[nodiscard]]
	base::OkBad createArchive(
		const artifacts::FileArtifact&              output,
		const std::vector<artifacts::FileArtifact>& inputs,
		const ArchivingOptions&                     options
	);
}
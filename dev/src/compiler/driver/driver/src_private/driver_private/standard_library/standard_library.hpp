/**
 * @file standard_library.hpp
 * @brief The private header for the standard library handling in the Duckling compiler,
 * for the code that is only used inside the driver.
 *
 * The role of these functions is to replicate what the package manager does by
 * adding the packages and dependencies of the standard library manually.
 */
#pragma once

#include <driver/options.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>

#include <array>

namespace compiler::driver {
	/**
	 * @brief Configuration for the standard library packages.
	 */
	struct STDPackageConfig final {
		std::string              name;
		std::string              subpath;
		std::vector<std::string> dependencies;
	};

	/// This is in header for the tests only
	extern const std::array<STDPackageConfig, 2> STD_PACKAGES_CONFIG;

	/**
	 * @brief Based on the `StdLibOptions` returns the path to the standard library, if it is
	 * used. If `DefaultStd` is used, it resolves the path to the standard library based on the
	 * executable path or if `STD_FIXED_PATH` is defined it uses that path.
	 */
	base::Optional<fs::FilePath> resolveStdPath(
		const options_types::StdLibOptions& standard_library_options
	);

	/**
	 * @brief Adds the standard library packages to the vector and adds std dependencies for
	 * all the packages in a vector.
	 * @param packages_info[out] The input vector is modified.
	 * @param std_path Path to the standard library.
	 * @param report Diagnostic reporter to report any issues with the standard library packages
	 * (like a missing package).
	 */
	base::OkBad addStandardLibraryPackages(
		std::vector<compiler::frontend::packages::RawPackageInfo>& packages_info,
		const fs::FilePath&                                        std_path,
		frontend::packages::DiagnosticReporter&                    report
	);

	/**
	 * @brief Based on the `StdLibOptions` returns the string with the arguments needed to
	 * link the standard library. Can be empty if the standard library is not used.
	 */
	base::Optional<std::string> getStdLibLinkingArgs(
		const options_types::StdLibOptions& linking_options
	);

	/**
	 * @brief The place where the compiled standard library binaries are placed.
	 */
	std::vector<artifacts::FileArtifact> getStdLibArtifacts();
}

#pragma once

#include <driver/options.hpp>
#include <driver/task/task.hpp>
#include <frontend/packages/packages.hpp>

#include "base/types/ok_bad.hpp"
#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>

namespace compiler::driver {
	base::Optional<fs::FilePath> resolveStdPath(
		const options_types::GlobalLinkingOptions& standard_library_options
	);

	base::OkBad addStandardLibraryPackages(
		const fs::FilePath& std_path, frontend::packages::DiagnosticReporter& report
	);

	base::OkBad addStandardLibraryDependencies(
		std::vector<compiler::frontend::packages::RawPackageInfo>& packages_info,
		frontend::packages::DiagnosticReporter&                    report
	);

	std::vector<PackageCompilationTask> getStandardLibraryCompilationTasks();

	base::Optional<std::string> getStdLibLinkingArgs(
		const options_types::GlobalLinkingOptions& linking_options
	);
}

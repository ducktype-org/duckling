#pragma once

#include <driver/options.hpp>
#include <frontend/packages/packages.hpp>

#include <base/collections/optional.hpp>
#include "base/types/ok_bad.hpp"

#include <filesystem/file_path.hpp>
#include <driver/task/task.hpp>

namespace compiler::driver {
	base::Optional<fs::FilePath> resolveStdPath(
		const options_types::StandardLibraryOptions& standard_library_options,
		frontend::packages::DiagnosticReporter&      report
	);

	void addStandardLibraryPackages(const fs::FilePath& std_path, frontend::packages::DiagnosticReporter&      report);

	base::OkBad addStandardLibraryDependencies(
		std::vector<compiler::frontend::packages::RawPackageInfo>& packages_info, frontend::packages::DiagnosticReporter&      report
	);

    std::vector<PackageCompilationTask> getStandardLibraryCompilationTasks();

    std::string getStdLibLinkingArgs();
}

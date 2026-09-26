#include "run.hpp"

#include "dk_emitter.hpp"
#include "manifest_emitter.hpp"
#include "package_writer.hpp"

#include <c_import/tu_reader.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <utility>
#include <vector>

namespace c_import {

	int run(const Options& options) {
		if (options.headers.empty()) {
			std::cerr << "error: at least one --header is required\n";
			return 1;
		}

		if (options.package_name.empty()) {
			std::cerr << "error: --package-name is required\n";
			return 1;
		}

		std::vector<std::string> absolute_headers;
		absolute_headers.reserve(options.headers.size());
		for (const auto& header: options.headers) {
			std::error_code error_code;
			auto            absolute = std::filesystem::absolute(header, error_code);
			if (error_code) {
				std::cerr << "error: cannot resolve " << header << ": " << error_code.message()
						  << '\n';
				return 1;
			}
			absolute_headers.emplace_back(absolute.string());
		}

		const auto read = readTranslationUnit(ReadRequest{ .headers    = absolute_headers,
		                                                   .clang_args = options.clang_args,
		                                                   .std_flag   = options.std_flag,
		                                                   .ignore_parse_errors
		                                                   = options.ignore_parse_errors });

		for (const auto& diagnostic: read.diagnostics) std::cerr << diagnostic << '\n';

		if (!read.error.empty()) {
			std::cerr << "error: " << read.error << '\n';
			return 1;
		}

		const auto manifest
			= emitManifest(ManifestInput{ .package_name    = options.package_name,
		                                  .version         = options.version,
		                                  .links           = options.links,
		                                  .dvm_shared_libs = options.dvm_shared_libs });
		if (!manifest.error.empty()) {
			std::cerr << "error: " << manifest.error << '\n';
			return 1;
		}

		const EmitterInput emitter_input{ .model      = read.model,
			                              .headers    = absolute_headers,
			                              .clang_args = options.clang_args };

		std::vector<PackageFile> modules;
		if (options.split) {
			const auto split = emitSplitBindings(emitter_input, options.package_name);
			for (const auto& module: split)
				modules.push_back(PackageFile{ .name = module.name, .contents = module.contents });
			// A header whose stem is the package name already owns that file, and its module
			// is the more useful one, so the aggregate is dropped rather than overwriting it.
			const bool name_taken = std::ranges::any_of(split, [&options](const auto& module) {
				return module.name == options.package_name;
			});
			if (!name_taken)
				modules.push_back(PackageFile{
					.name     = options.package_name,
					.contents = emitAggregateModule(options.package_name, split) });
		} else {
			modules.push_back(PackageFile{ .name     = options.package_name,
			                               .contents = emitBindings(emitter_input) });
		}

		const PackageLayout layout{
			.package_name  = options.package_name,
			.manifest_yaml = manifest.yaml,
			.root_module   = emitRootModule(options.package_name),
			.modules       = std::move(modules),
		};

		const std::filesystem::path out_dir
			= options.out_dir.empty() ? options.package_name : options.out_dir;

		if (const auto written = writePackage(out_dir, layout, options.force);
		    !written.error.empty()) {
			std::cerr << "error: " << written.error << '\n';
			return 1;
		}

		std::cout << "Wrote package `" << options.package_name << "` to " << out_dir.string()
				  << "\nImport it with `import " << options.package_name << '.'
				  << options.package_name << ";`\n";
		return 0;
	}

}

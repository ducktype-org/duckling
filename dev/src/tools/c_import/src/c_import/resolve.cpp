// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "resolve.hpp"

#include "process.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <sstream>

namespace c_import {

	namespace {

		// The directories duck searches for `external-libs` anyway; keep in sync with
		// `DEFAULT_EXTERNAL_LIB_PATHS` in duck's `unit_runner/external_libs.rs`.
#if defined(__APPLE__)
		constexpr auto DEFAULT_LIB_PATHS
			= std::to_array<std::string_view>({ "/usr/local/lib", "/opt/homebrew/lib", "/usr/lib" });
#elif defined(__linux__) && defined(__x86_64__)
		constexpr auto DEFAULT_LIB_PATHS = std::to_array<std::string_view>({
			"/usr/local/lib",
			"/usr/lib",
			"/lib",
			"/usr/lib64",
			"/lib64",
			"/usr/lib/x86_64-linux-gnu",
			"/lib/x86_64-linux-gnu",
		});
#elif defined(__linux__) && defined(__aarch64__)
		constexpr auto DEFAULT_LIB_PATHS = std::to_array<std::string_view>({
			"/usr/local/lib",
			"/usr/lib",
			"/lib",
			"/usr/lib64",
			"/lib64",
			"/usr/lib/aarch64-linux-gnu",
			"/lib/aarch64-linux-gnu",
		});
#elif defined(__linux__)
		constexpr auto DEFAULT_LIB_PATHS = std::to_array<std::string_view>(
			{ "/usr/local/lib", "/usr/lib", "/lib", "/usr/lib64", "/lib64" }
		);
#else
		constexpr std::array<std::string_view, 0> DEFAULT_LIB_PATHS{};
#endif

		std::vector<std::string> words(std::string_view text) {
			std::vector<std::string> result;
			std::istringstream       in{ std::string{ text } };
			for (std::string word; in >> word;) result.push_back(word);
			return result;
		}

		std::expected<std::vector<std::string>, std::string> pkgConfig(
			const std::vector<std::string>& args
		) {
			auto output = runProcess("pkg-config", args);
			if (!output) return std::unexpected(output.error() + "; is pkg-config installed?");
			if (output->exit_code != 0) {
				std::string joined;
				for (const auto& arg: args) joined += " " + arg;
				auto err = output->err;
				while (!err.empty() && (err.back() == '\n' || err.back() == ' ')) err.pop_back();
				return std::unexpected(std::format("`pkg-config{}` failed: {}", joined, err));
			}
			return words(output->out);
		}

		void pushUnique(std::vector<std::string>& list, const std::string& value) {
			if (!std::ranges::contains(list, value)) list.push_back(value);
		}

		/// Adds a library directory, relative to `root` or absolute (from pkg-config).
		void addLibPath(
			Resolved& resolved, const std::string& dir, const std::filesystem::path& root
		) {
			std::filesystem::path path{ dir };
			if (!path.is_absolute()) {
				pushUnique(resolved.external_lib_paths, dir);
				return;
			}
			auto normal = path.lexically_normal();
			if (std::ranges::any_of(DEFAULT_LIB_PATHS, [&](std::string_view known) {
					return std::filesystem::path{ known } == normal;
				}))
				return;
			auto inside
				= normal.lexically_relative(std::filesystem::absolute(root).lexically_normal());
			if (!inside.empty() && *inside.begin() != "..") {
				pushUnique(resolved.external_lib_paths, relativePath(normal, root));
				return;
			}
			resolved.warnings.push_back(std::format(
				"the library directory `{}` is specific to this machine; the package only builds "
				"elsewhere if the library is installed in a standard location",
				dir
			));
			pushUnique(resolved.external_lib_paths, dir);
		}

		std::expected<void, std::string> addLibrary(
			Resolved& resolved, const std::string& library, const std::filesystem::path& root
		) {
			if (library.starts_with("-L")) {
				addLibPath(resolved, library.substr(2), root);
			} else if (library.starts_with("-l")) {
				if (library.starts_with("-l:"))
					resolved.warnings.push_back(std::format(
						"`{}` names an exact file, which `external-libs` cannot express yet "
						"(#4140); it was left out",
						library
					));
				else
					pushUnique(resolved.external_libs, library.substr(2));
			} else if (library.starts_with('-')) {
				resolved.warnings.push_back(std::format(
					"the linker flag `{}` has no `external-libs` equivalent; it was left out",
					library
				));
			} else {
				std::filesystem::path path{ library };
				auto                  name = libraryName(path.filename().string());
				if (name.empty())
					return std::unexpected(std::format(
						"`{}` is not a library; duckbind links `lib<name>.a`, `.so` or `.dylib` "
						"files, so wrap object files in an archive with `ar`",
						library
					));
				auto dir = path.parent_path().string();
				addLibPath(resolved, dir.empty() ? "." : dir, root);
				pushUnique(resolved.external_libs, name);
			}
			return {};
		}

		/**
		 * @brief Flags reading the headers like the system C compiler, which links Duckling
		 * programs: its target (libclang's own default is a glibc target even on musl systems)
		 * and, on macOS, the SDK, which libclang does not find by itself.
		 */
		void addHostFlags(std::vector<std::string>& cflags) {
			auto first_line = [](const std::string& program, const std::vector<std::string>& args) {
				auto output = runProcess(program, args);
				if (!output || output->exit_code != 0) return std::string{};
				auto line = output->out.substr(0, output->out.find('\n'));
				while (!line.empty() && line.back() == ' ') line.pop_back();
				return line;
			};
#if defined(__APPLE__)
			if (std::ranges::none_of(cflags, [](const auto& f) {
					return f.starts_with("-isysroot");
				})) {
				if (auto sdk = first_line("xcrun", { "--show-sdk-path" }); !sdk.empty()) {
					cflags.emplace_back("-isysroot");
					cflags.push_back(sdk);
				}
			}
#else
			if (std::ranges::any_of(cflags, [](const auto& f) {
					return f == "-target" || f.starts_with("--target=");
				}))
				return;
			for (const char* compiler: { "gcc", "cc" }) {
				if (auto triple = first_line(compiler, { "-dumpmachine" }); !triple.empty()) {
					cflags.push_back("--target=" + triple);
					return;
				}
			}
#endif
		}

	}

	std::expected<Resolved, std::string> resolve(
		const Recipe& recipe, const std::filesystem::path& root, bool host_flags
	) {
		Resolved                 resolved;
		std::vector<std::string> libraries;
		if (!recipe.pkg_config.empty()) {
			std::vector<std::string> args{ "--cflags" };
			args.insert(args.end(), recipe.pkg_config.begin(), recipe.pkg_config.end());
			auto cflags = pkgConfig(args);
			if (!cflags) return std::unexpected(cflags.error());
			resolved.cflags = std::move(*cflags);
			args.front()    = "--libs";
			auto libs       = pkgConfig(args);
			if (!libs) return std::unexpected(libs.error());
			libraries = std::move(*libs);
			for (const auto& package: recipe.pkg_config) {
				auto version = pkgConfig({ "--modversion", package });
				if (!version) return std::unexpected(version.error());
				std::string joined;
				for (const auto& word: *version) joined += (joined.empty() ? "" : " ") + word;
				resolved.versions.emplace_back(package, joined);
			}
		}
		resolved.cflags.insert(resolved.cflags.end(), recipe.cflags.begin(), recipe.cflags.end());
		if (host_flags) addHostFlags(resolved.cflags);
		libraries.insert(libraries.end(), recipe.libraries.begin(), recipe.libraries.end());
		for (const auto& library: libraries)
			if (auto added = addLibrary(resolved, library, root); !added)
				return std::unexpected(added.error());
		return resolved;
	}

	std::string libraryName(std::string_view file_name) {
		if (!file_name.starts_with("lib")) return {};
		auto             stem = file_name.substr(3);
		std::string_view name;
		if (stem.ends_with(".a"))
			name = stem.substr(0, stem.size() - 2);
		else if (stem.ends_with(".dylib"))
			name = stem.substr(0, stem.size() - 6);
		else if (auto so = stem.find(".so");
		         so != std::string_view::npos && (so + 3 == stem.size() || stem[so + 3] == '.'))
			name = stem.substr(0, so);
		return std::string{ name };
	}

}

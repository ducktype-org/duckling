// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "verify.hpp"

#include "package.hpp"
#include "process.hpp"

#include <charconv>
#include <format>
#include <optional>
#include <ranges>

namespace c_import {

	namespace {

		/// The last line of `out` that is a number: what the check prints before failing.
		std::optional<std::size_t> mismatchIndex(std::string_view out) {
			std::optional<std::size_t> last;
			for (auto line: std::views::split(out, '\n')) {
				std::string_view text{ line.begin(), line.end() };
				while (!text.empty() && (text.back() == '\r' || text.back() == ' '))
					text.remove_suffix(1);
				std::size_t value = 0;
				auto [end, ec]    = std::from_chars(text.data(), text.data() + text.size(), value);
				if (ec == std::errc{} && end == text.data() + text.size() && !text.empty())
					last = value;
			}
			return last;
		}

	}

	std::expected<void, std::string> verify(
		const VerifyOptions& options, const std::function<void(const std::string&)>& info
	) {
		TempDir    dir;
		const auto app = dir.path() / "duckbind_check";
		auto       package_path
			= std::filesystem::absolute(options.package_root).lexically_normal().string();
		if (package_path.ends_with('/')) package_path.pop_back();

		auto manifest = writeFile(
			app / "quackconfig.yaml",
			std::format(
				"metadata:\n  name: duckbind_check\n  version: '1.0.0'\n"
				"dependencies:\n  {}:\n    source:\n      path: '{}'\n"
				"profiles:\n  dvm:\n    dvm-bytecode: true\n",
				options.name,
				package_path
			)
		);
		if (!manifest) return manifest;
		auto program = writeFile(
			app / "src" / "src.dk",
			options.layout_check
				+ "\nfun main() -> i64 = {\n"
				  "    let mismatch = c_layout_mismatch();\n"
				  "    if (mismatch == 0i64) { return 0i64; }\n"
				  "    builtin_output_i64(mismatch);\n"
				  "    return 1i64;\n"
				  "}\n"
		);
		if (!program) return program;

		struct Backend final {
			const char* name;
			const char* profile;
			bool        enabled;
		};

		for (const auto& backend:
		     { Backend{ .name = "native", .profile = "dev", .enabled = options.native },
		       Backend{ .name = "DVM", .profile = "dvm", .enabled = options.dvm } }) {
			if (!backend.enabled) continue;
			info(std::format("checking the package on the {} backend", backend.name));
			auto run = runProcess(
				options.duck,
				{ "-C", app.string(), "run", std::format("--profile={}", backend.profile) }
			);
			if (!run)
				return std::unexpected(
					run.error() + "; duckbind checks packages with duck, so put it on PATH or "
					              "pass `--duck`"
				);
			if (run->exit_code == 0) continue;
			if (auto index = mismatchIndex(run->out);
			    index && *index >= 1 && *index <= options.layouts.size())
				return std::unexpected(std::format(
					"on the {} backend, the size or alignment of `{}` differs from the C one; "
					"this is a bug in duckbind",
					backend.name,
					options.layouts[*index - 1]
				));
			return std::unexpected(std::format(
				"the package does not build or run on the {} backend; this is a bug in duckbind "
				"unless the library itself is missing; the package is kept for inspection.\n{}{}",
				backend.name,
				run->out,
				run->err
			));
		}
		return {};
	}

}

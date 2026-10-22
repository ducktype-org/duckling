// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <version>  // IWYU pragma: keep

#ifdef __cpp_lib_stacktrace

	#include "pretty_stacktrace.hpp"

	#include <filesystem>

namespace base {
	namespace {
		std::string col(const std::string& color_number, const std::string& text) {
			std::string out;
			out += "\033[" + color_number + "m";
			out += text;
			out += "\033[0m";
			return out;
		}
	}

	std::string prettyStacktraceString(const std::stacktrace& stack) {
		std::string out;
		int         i = 0;
		for (auto& entry: stack) {
			auto path = std::filesystem::relative(entry.source_file());

			out += "    " + std::to_string(i) + "# ";
			out += col("35", entry.description()) + "\n";

			if (!path.empty()) {
				out += "     at " + col("36", path.string()) + ":";
				out += col("32", std::to_string(entry.source_line())) + "\n";
			}

			i++;
		}
		return out;
	}
}

#endif

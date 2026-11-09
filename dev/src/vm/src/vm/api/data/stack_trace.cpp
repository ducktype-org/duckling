// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "stack_trace.hpp"

#include <sstream>

namespace vm::api {
	namespace {
		void writeLocation(std::ostream& out, const SourceLocation& location) {
			out << location.file << ":" << location.line;
			if_opt_some(location.column, column) { out << ":" << column; }
		}
	}

	std::string formatStackTrace(const StackTrace& trace) {
		if (trace.frames.empty()) return "";

		std::stringstream out;
		out << "DVM stack trace (most recent call first):\n";
		u64 expected_depth = 0;
		for (const auto& frame: trace.frames) {
			if (frame.depth > expected_depth)
				out << "  ... " << frame.depth - expected_depth << " frames omitted ...\n";
			expected_depth = frame.depth + 1;

			out << "  #" << frame.depth << " ";
			if (frame.source_name.has_value())
				out << *frame.source_name;
			else
				out << frame.function_name.strView();

			if (frame.call_site.has_value()) {
				out << " at ";
				writeLocation(out, *frame.call_site);
			} else if (frame.definition.has_value()) {
				out << " (defined at ";
				writeLocation(out, *frame.definition);
				out << ")";
			}
			out << "\n";
		}
		return out.str();
	}
}

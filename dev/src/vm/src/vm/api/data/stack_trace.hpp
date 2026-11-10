// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <json/json.hpp>
#include <json/optional.hpp>
#include <json/string_id.hpp>

#include <ostream>
#include <string>
#include <vector>

namespace vm::api {
	/**
	 * @brief A place in a source file: a Duckling file, or a bytecode file when the code has no
	 * debug info.
	 */
	struct SourceLocation final {
		std::string file;
		u64         line = 0;
		/// Only known for a place in a bytecode file.
		base::Optional<u64> column;

		bool operator==(const SourceLocation&) const = default;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(SourceLocation, file, line, column);
	};

	/**
	 * @brief One frame of the DVM call stack.
	 */
	struct StackFrame final {
		/// How far the frame is from the most recent call: 0 for the innermost frame.
		u64 depth = 0;
		/// The name of the function in the bytecode (its mangled name).
		base::StrID function_name;
		/// The name of the function in the Duckling source, when the process knows it.
		base::Optional<std::string> source_name;
		/// Where the frame is in its function: the call it is waiting on. The innermost frame
		/// has no saved position, so it has none.
		base::Optional<SourceLocation> call_site;
		/// Where the function is defined, when the process knows it.
		base::Optional<SourceLocation> definition;

		bool operator==(const StackFrame&) const = default;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(
			StackFrame, depth, function_name, source_name, call_site, definition
		);
	};

	/**
	 * @brief The DVM call stack of a thread at the moment of a runtime error.
	 */
	struct StackTrace final {
		/// Most recent call first. A very deep stack keeps only its two ends, so `depth` skips
		/// the frames left out in the middle.
		std::vector<StackFrame> frames;

		/**
		 * @brief Writes the trace for people to `out`, one line per frame, most recent call
		 * first. A frame is named by its source name when it has one, else by its mangled name.
		 * It shows its call site when it has one, else where its function is defined. Frames
		 * left out in the middle get one `... N frames omitted ...` line.
		 * Writes nothing when the trace has no frames.
		 */
		void print(std::ostream& out) const;

		bool operator==(const StackTrace&) const = default;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(StackTrace, frames);
	};
}

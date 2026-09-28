/**
 * @file tu_reader.hpp
 * @brief Reads C headers into a `CModel` with libclang.
 */
#pragma once

#include "c_model.hpp"

#include <string>
#include <vector>

namespace c_import {

	struct ReadOptions final {
		/// Headers to read: an existing file path, or a name found on the include path
		/// (`SDL3/SDL.h`).
		std::vector<std::string> headers;
		/// Extra compiler arguments, such as `-I` and `-D` flags.
		std::vector<std::string> clang_args;
		/// clang's resource directory, holding its builtin headers (`stddef.h`, ...).
		std::string resource_dir;
	};

	struct ReadResult final {
		CModel model;
		/// Compiler errors of the headers; the model is incomplete when there are any.
		std::vector<std::string> errors;
		/// How many object-like macros of the requested headers did not evaluate to a number.
		std::size_t non_numeric_macros = 0;
	};

	/**
	 * @brief Parses every requested header as one translation unit.
	 *
	 * A declaration counts as coming from the requested headers when its file is in the
	 * directory (or below the directory) of one of them, so the headers an umbrella header like
	 * `SDL3/SDL.h` includes count too.
	 */
	ReadResult readHeaders(const ReadOptions& options);

}

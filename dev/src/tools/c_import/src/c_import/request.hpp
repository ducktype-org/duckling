/**
 * @file request.hpp
 * @brief The JSON contract between `duck translate-c` and `duck_c_import`.
 */
#pragma once

#include "dk_model.hpp"
#include "lower.hpp"
#include "tu_reader.hpp"

#include <expected>
#include <string>
#include <vector>

namespace c_import {

	struct Request final {
		ReadOptions  read;
		LowerOptions lower;
		/// Stem of the bindings module file, e.g. `sdl3` for `sdl3.dk`.
		std::string module_name;
		/// Import path of the bindings module, used by the layout check module.
		std::string bindings_import;
		/// Directory the modules are written to.
		std::string output_dir;
		/// Lines put, as comments, at the top of every generated file.
		std::vector<std::string> banner;
	};

	struct Report final {
		std::vector<std::string> errors;
		std::vector<std::string> files;
		DkModule                 module;
		std::size_t              non_numeric_macros = 0;
	};

	std::expected<Request, std::string> parseRequest(std::string_view json);

	std::string serializeReport(const Report& report);

	/**
	 * @brief Reads the headers, lowers them and writes the modules into `output_dir`.
	 * @note Nothing is written when the headers do not compile.
	 */
	Report run(const Request& request);

}

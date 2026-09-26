#pragma once

#include <c_import/c_decls.hpp>

#include <string>
#include <vector>

namespace c_import {

	struct ReadRequest final {
		std::vector<std::string> headers;
		std::vector<std::string> clang_args;
		std::string              std_flag;
		bool                     ignore_parse_errors;
	};

	struct ReadResult final {
		TranslationUnitModel     model;
		std::vector<std::string> diagnostics;
		/** Empty on success. */
		std::string error;
	};

	/** Parses every requested header as one synthesized translation unit. */
	ReadResult readTranslationUnit(const ReadRequest& request);

}

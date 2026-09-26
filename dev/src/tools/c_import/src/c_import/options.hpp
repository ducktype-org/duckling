#pragma once

#include <string>
#include <vector>

namespace c_import {

	struct Options final {
		std::vector<std::string> headers;
		std::string              package_name;
		std::string              out_dir;
		/** Passed to the linker verbatim on the native backend. */
		std::string links;
		/** Set when --library (rather than --links-raw) held a space. */
		bool                     library_has_space = false;
		std::vector<std::string> dvm_shared_libs;
		std::string              version;
		std::string              std_flag;
		/** Emit one module per translated header instead of a single one. */
		/** Compile the generated package to prove it is valid before reporting success. */
		bool verify = true;
		/** Compiler used for that check, resolved through $PATH by default. */
		std::string duckc               = "duckc";
		bool        split               = false;
		bool        force               = false;
		bool        ignore_parse_errors = false;
		/** Everything after a bare `--`, handed to clang untouched. */
		std::vector<std::string> clang_args;
	};

	struct SplitArguments final {
		std::vector<std::string> own;
		std::vector<std::string> clang;
	};

	/**
	 * @brief Splits argv at the first bare `--`.
	 *
	 * clah has no passthrough of its own, so the clang half is taken out before parsing.
	 */
	SplitArguments splitArguments(int argc, const char* const* argv);

}

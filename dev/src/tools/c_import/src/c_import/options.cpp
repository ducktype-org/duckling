#include "options.hpp"

#include <string_view>

namespace c_import {

	SplitArguments splitArguments(int argc, const char* const* argv) {
		SplitArguments split;
		bool           in_clang_half = false;

		for (int i = 0; i < argc; i++) {
			const std::string_view argument = argv[i];
			if (!in_clang_half && argument == "--") {
				in_clang_half = true;
				continue;
			}

			if (in_clang_half)
				split.clang.emplace_back(argument);
			else
				split.own.emplace_back(argument);
		}

		return split;
	}

}

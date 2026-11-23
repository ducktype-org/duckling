// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "generate.hpp"

#include "emit.hpp"

#include <format>
#include <ranges>
#include <vector>

namespace c_import {

	std::string abiTriple(std::string_view triple) {
		std::vector<std::string_view> parts;
		for (auto part: std::views::split(triple, '-'))
			parts.emplace_back(part.begin(), part.end());
		if (parts.size() == 3 && parts[1] != "apple") parts.insert(parts.begin() + 1, "unknown");
		if (parts.size() == 4 && parts[1] != "apple") parts[1] = "unknown";
		std::string result;
		for (auto part: parts) result += std::format("{}{}", result.empty() ? "" : "-", part);
		return result;
	}

	Generated generate(GenerateOptions options) {
		if (options.read.resource_dir.empty()) options.read.resource_dir = DUCK_CLANG_RESOURCE_DIR;

		Generated generated;
		auto      read               = readHeaders(options.read);
		generated.errors             = std::move(read.errors);
		generated.non_numeric_macros = read.non_numeric_macros;
		generated.target_triple      = read.model.target_triple;
		if (!generated.errors.empty()) return generated;

		generated.module = lower(read.model, options.lower);
		auto banner      = options.banner;
		banner.push_back(std::format("Target: {}.", abiTriple(generated.target_triple)));
		generated.bindings     = emitModule(generated.module, banner);
		generated.layout_check = emitLayoutCheck(generated.module, banner, options.bindings_import);
		return generated;
	}

}

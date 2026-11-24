// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/module_tree/source_file.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <string>

namespace duck_ls {

	/**
	 * @brief The hover text for the code at `offset` in `file`, as Markdown.
	 *
	 * @param offset A code point offset, as `toCodePointOffset` computes it.
	 * @return Empty when there is nothing to describe. Also empty when the compiler throws while
	 * computing the hover; the exception is logged. A compiler panic throws only in Dev builds,
	 * so in Release builds it is not caught here.
	 */
	base::Optional<std::string> hoverMarkdownAt(
		base::Ref<compiler::frontend::SourceFile> file, usize offset
	);

}

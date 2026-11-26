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

#include <diagnostic/source_position.hpp>

#include <string>

namespace duck_ls {

	/**
	 * @brief What a hover shows: the text, and where the thing it describes is in the file.
	 */
	struct HoverContent final {
		/// The hover text, as Markdown.
		std::string markdown;
		/// Where the described expression is, so that the editor can highlight it. Empty for
		/// a declared name, where the editor highlights the word under the cursor by itself.
		base::Optional<dia::SourcePosition> position{};
	};

	/**
	 * @brief The hover for the code at `offset` in `file`.
	 *
	 * @param offset A code point offset, as `toCodePointOffset` computes it.
	 * @return Empty when there is nothing to describe. Also empty when the compiler throws while
	 * computing the hover; the exception is logged. A compiler panic throws only in Dev builds,
	 * so in Release builds it is not caught here.
	 */
	base::Optional<HoverContent> hoverAt(
		base::Ref<compiler::frontend::SourceFile> file, usize offset
	);

}
